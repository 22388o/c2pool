#pragma once

#include <map>
#include <queue>
#include <list>
#include <memory>
#include <tuple>
#include <btclibs/uint256.h>
#include <btclibs/arith_uint256.h>
#include <libcoind/data.h>
#include <boost/function.hpp>
#include "share.h"
using namespace std;

namespace c2pool::shares
{
    class element_type
    {
    public:
        map<uint256, element_type>::iterator prev;
        list<map<uint256, element_type>::iterator> nexts;
        shared_ptr<BaseShare> element;

        int32_t height;
        arith_uint256 work;
        arith_uint256 min_work;

    public:
        element_type() {}
        element_type(shared_ptr<BaseShare> _share)
        {
            element = _share;
            work = coind::data::target_to_average_attempts(_share->target);
            min_work = coind::data::target_to_average_attempts(_share->max_target);
            height = 1;
        }

        uint256 hash()
        {
            return element->hash;
        }

        uint256 prev_hash()
        {
            if (element == nullptr)
            {
                uint256 res;
                res.SetNull();
                return res;
            }
            return element->previous_hash;
        }

        element_type operator+(const element_type &element)
        {
            element_type res = *this;
            res.work += element.work;
            res.height += element.height;
            return res;
        }

        element_type operator-(const element_type &element)
        {
            element_type res = *this;
            res.work -= element.work;
            res.height -= element.height;
            return res;
        }

        element_type operator+=(const element_type &element)
        {
            this->work += element.work;
            this->height += element.height;
            return *this;
        }

        element_type operator-=(const element_type &element)
        {
            this->work -= element.work;
            this->height -= element.height;
            return *this;
        }

        // static element_type get_null(){}
    };

    class element_delta_type
    {
    private:
        bool _none;

    public:
        uint256 head;
        uint256 tail;
        int32_t height;
        arith_uint256 work;
        arith_uint256 min_work;

        element_delta_type(bool none = true)
        {
            _none = none;
        }

        element_delta_type(element_type &el)
        {
            head = el.hash();
            tail = el.prev_hash();
            height = el.height;
            work = el.work;
            min_work = el.min_work;
        }

        element_delta_type operator-(const element_delta_type &el) const
        {
            element_delta_type res = *this;
            res.tail = el.head;
            res.height -= el.height;
            res.work -= el.work;
            res.min_work -= el.min_work;
            return res;
        }

        void operator-=(const element_delta_type &el)
        {
            tail = el.head;
            height -= el.height;
            work -= el.work;
            min_work -= el.min_work;
        }

        bool is_none()
        {
            return _none;
        }

        void set_none(bool none = true)
        {
            _none = none;
        }
    };

    class PrefsumShare
    {
    public:
        map<uint256, shared_ptr<BaseShare>> items;
        map<uint256, element_type> sum;

    protected:
        element_type make_element(shared_ptr<BaseShare> _share)
        {
            element_type element(_share);
            element.prev = sum.find(_share->previous_hash);
            return element;
        }

    public:
        void add(shared_ptr<BaseShare> _share)
        {
            //TODO: throw if share exists in items;
            items[_share->hash] = _share;

            element_type new_sum_element = make_element(_share); //only this element
            if (new_sum_element.prev != sum.end())
            {
                new_sum_element += new_sum_element.prev->second;
                sum[_share->hash] = new_sum_element;
                new_sum_element.prev->second.nexts.push_back(sum.find(_share->hash));
            }
            else
            {
                sum[_share->hash] = new_sum_element;
            }
        }

#define set_cur_it(new_value) \
    cur_it = new_value;       \
    cur_it_nexts = cur_it->second.nexts

#define get_q() \
    q.front();  \
    q.pop()

        void remove(uint256 hash)
        {
            //TODO: throw if hash not exists in items;
            auto _share = items[hash];
            items.erase(hash);

            queue<map<uint256, element_type>::iterator> q;
            auto remover = sum[hash];

            auto cur_it = sum.find(hash);
            auto &cur_it_nexts = cur_it->second.nexts;

            if (remover.nexts.empty())
            {
                cur_it = sum.end();
            }
            else
            {
                for (auto item : cur_it_nexts)
                {
                    item->second.prev = sum.end(); //remover.prev;
                    q.push(item);
                }
                auto new_it = get_q();
                set_cur_it(new_it);
            }
            //добавлять все nexts в текущей cur_it в q и проходить их.
            while (!q.empty() || (cur_it != sum.end()))
            {
                if (cur_it != sum.end())
                {
                    cur_it->second -= remover;
                    for (auto item : cur_it_nexts)
                    {
                        q.push(item);
                    }
                }
                if (!q.empty())
                {
                    auto new_it = get_q();
                    set_cur_it(new_it);
                }
                else
                {
                    break;
                }
            }
            sum.erase(hash);
        }
#undef set_cur_it
#undef get_q

        bool is_child_of(uint256 item_hash, uint256 possible_child_hash)
        {
            auto _it = sum.find(item_hash);
            if (sum.find(possible_child_hash) == sum.end())
            {
                throw invalid_argument("is_child_of: possible_child_hash existn't in tracker");
            }
            while (_it != sum.end())
            {
                if (_it->second.prev->first == possible_child_hash)
                    return true;
                else
                    _it = _it->second.prev;
            }
            return false;
        }

        element_delta_type get_delta(uint256 first_hash, uint256 second_hash)
        {
            if (sum.find(first_hash) == sum.end())
            {
                throw invalid_argument("first_hash in get_delta existn't in sum!");
            }
            if (sum.find(second_hash) == sum.end())
            {
                throw invalid_argument("second in get_delta existn't in sum!");
            }

            if (!is_child_of(first_hash, second_hash))
            {
                throw invalid_argument("get_delta: second_hash is not child for first_hash!");
                //return element_delta_type();
            }

            element_delta_type fir(sum[first_hash]);
            element_delta_type sec(sum[second_hash]);
            return fir - sec;
        }

        //todo: create optimization like get_last_iterator
        uint256 get_last(uint256 hash)
        {
            auto _it = sum.find(hash);
            if (_it == sum.end())
            {
                throw invalid_argument("[get_last] hash not exists in sum");
            }
            //cout << _it->second.element;
            auto last = _it->second.prev_hash();
            while (_it != sum.end())
            {
                last = _it->second.prev_hash();
                _it = _it->second.prev;
                // this_thread::sleep_for(chrono::milliseconds(1));
            }
            return last;
        }

        element_delta_type get_delta_to_last(uint256 hash)
        {
            auto el = sum.find(hash);
            if (el == sum.end())
            {
                throw invalid_argument("[get_delta_to_last] hash not exists in sum");
            }
            element_delta_type delta(el->second);
            delta.tail = get_last(hash);
            return delta;
        }

        uint256 get_work(uint256 hash)
        {
            return ArithToUint256(get_delta_to_last(hash).work);
        }

        int32_t get_height(uint256 hash)
        {
            return get_delta_to_last(hash).height;
        }

        //todo: get_best
        uint256 get_test_best()
        {
            if (items.empty())
            {
                uint256 res;
                res.SetNull();
                return res;
            }
            auto best = items.begin()->second->hash;
            for (auto item : items)
            {
                if (item.second->hash > best)
                    best = item.second->hash;
            }
            return best;
        }

        bool exists(uint256 hash)
        {
            return (items.find(hash) != items.end());
        }

        tuple<int32_t, uint256> get_height_and_last(uint256 hash)
        {
            auto delta = get_delta_to_last(hash);
            return std::make_tuple(delta.height, delta.tail);
        }

        //TODO: TEST
        boost::function<bool(uint256 &ref_hash)> get_chain(uint256 hash, int32_t n)
        {
            return [&, this]()
            {
                auto cur_it = items.find(hash);
                auto cur_pos = n; //exclusive 0
                auto &_items = this->items;
                return [=, &_items](uint256 &ref_hash) mutable
                {
                    if ((cur_it != _items.end()) && (cur_pos != 0))
                    {
                        ref_hash = cur_it->second->hash;
                        cur_it = _items.find(cur_it->second->previous_hash);
                        cur_pos -= 1;
                        return true;
                    }
                    else
                    {
                        return false;
                    }
                };
            }();
        }

        virtual uint256 get_nth_parent_hash(uint256 hash, int32_t n)
        {
            auto it = sum.find(hash);
            int32_t dist = 0;
            while ((dist != n) && (it != sum.end()))
            {
                dist += 1;
                if (dist == n)
                {
                    break;
                }
                it = it->second.prev;
            }
            if (it == sum.end())
            {
                throw invalid_argument((boost::format("in get_nth_parent_hash(%1%, %2%): n < chain for this hash") % hash.ToString() % n).str());
            }
            assert(dist == n);
            return it->second.prev_hash();
        }
        //todo: remove range
    };

    class PrefsumVerifiedShare : public PrefsumShare
    {
    protected:
        PrefsumShare &_prefsum_share;

    public:
        PrefsumVerifiedShare(PrefsumShare &prefsum_share) : PrefsumShare(), _prefsum_share(prefsum_share)
        {
        }

        uint256 get_nth_parent_hash(uint256 hash, int32_t n) override
        {
            return _prefsum_share.get_nth_parent_hash(hash, n);
        }
    };
}