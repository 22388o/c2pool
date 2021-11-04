#pragma once

#include <sstream>
#include <vector>
#include <numeric>
using namespace std;

class PackStream;

template <typename T>
concept StreamObjType = requires(T a, PackStream &stream)
{
    {
        a.write(stream)
        } -> PackStream &;

    {
        a.read(stream)
        } -> PackStream &;
};

template <typename T>
concept C_STRING = std::is_same_v<T, const char *> || std::is_same_v<T, char *> || std::is_same_v<T, unsigned char *> || std::is_same_v<T, const unsigned char *> || std::is_same_v<T, char>;

template <typename T>
concept StreamIntType = !C_STRING<T> && std::numeric_limits<T>::is_integer;

template <typename T>
concept StreamEnumType = std::is_enum_v<T>;

struct BaseMaker //TIP, HACK, КОСТЫЛЬ
{
};

template <typename T>
concept bool MakerType = std::is_base_of_v<BaseMaker, T>;

template <typename T, typename SUB_T>
struct Maker : BaseMaker
{
    typedef T to;
    typedef SUB_T from;

    template <typename LAST_T>
    static T make_type(LAST_T v)
    {
        return T(v);
    }

    template <typename LAST_T>
    static vector<T> make_list_type(vector<LAST_T> v_arr)
    {
        vector<T> res;
        for (auto v : v_arr)
        {
            res.push_back(make_type(v));
        }
        return res;
    }
};

template <typename T, MakerType SUB_T>
struct Maker<T, SUB_T> : BaseMaker
{
    typedef T to;
    typedef SUB_T from;

    template <typename LAST_T>
    static T make_type(LAST_T v)
    {
        return SUB_T::make_type(v);
    }

    template <typename LAST_T>
    static vector<T> make_list_type(vector<LAST_T> v_arr)
    {
        vector<T> res;
        for (auto v : v_arr)
        {
            res.push_back(make_type(v));
        }
        return res;
    }
};

template <typename LIST_TYPE>
struct MakerListType : BaseMaker
{
    static vector<LIST_TYPE> make_type(vector<LIST_TYPE> values)
    {
        return values;
    }
};

template <MakerType LIST_TYPE>
struct MakerListType<LIST_TYPE> : BaseMaker
{
    template <typename SUB_EL_TYPE>
    static vector<LIST_TYPE> make_type(vector<SUB_EL_TYPE> values)
    {
        return LIST_TYPE::make_list_type(values);
    }
};

struct PackStream
{
    vector<unsigned char> data;

    PackStream() {}

    PackStream(vector<unsigned char> value)
    {
        data = value;
    }

    PackStream(unsigned char *value, int32_t len)
    {
        data = vector<unsigned char>(value, value + len);
    }

    PackStream(char *value, int32_t len)
    {
        auto temp = (unsigned char *)value;
        data = vector<unsigned char>(temp, temp + len);
    }

    PackStream &operator<<(PackStream &val)
    {
        data.insert(data.end(), val.data.begin(), val.data.end());
        return *this;
    }

    template <typename T>
    PackStream &operator<<(T &val)
    {
        unsigned char *packed = reinterpret_cast<unsigned char *>(&val);
        int32_t len = sizeof(val) / sizeof(*packed);

        data.insert(data.end(), packed, packed + len);
        return *this;
    }

    template <typename T>
    PackStream &operator<<(T val[])
    {
        unsigned char *packed = reinterpret_cast<unsigned char *>(&val);
        int32_t len = sizeof(val) / sizeof(*packed);
        data.insert(data.end(), packed, packed + len);
        return *this;
    }

    template <StreamObjType T>
    PackStream &operator<<(T &val)
    {
        val.write(*this);
        return *this;
    }

#define ADDING_INT(num_type, _code)                                   \
    unsigned char code = _code;                                       \
    data.push_back(code);                                             \
    num_type val2 = (num_type)val;                                    \
    unsigned char *packed = reinterpret_cast<unsigned char *>(&val2); \
    int32_t len = sizeof(val2) / sizeof(*packed);                     \
    data.insert(data.end(), packed, packed + len);                    \
    return *this;

    template <StreamIntType T>
    PackStream &operator<<(T &val)
    {
        if (val < 0xfd)
        {
            unsigned char packed = (unsigned char)val;
            data.push_back(packed);
            return *this;
        }
        if (val < 0xffff)
        {
            ADDING_INT(uint16_t, 0xfd)
        }
        if (val < 0xffffffff)
        {
            ADDING_INT(uint32_t, 0xfe)
        }
        if (val < 0xffffffffffffffff)
        {
            ADDING_INT(uint64_t, 0xff)
        }

        throw std::invalid_argument("int too large for varint");
    }
#undef ADDING_INT

#define CALC_SIZE(T) (sizeof(T))

    PackStream &operator>>(PackStream &val)
    {
        val.data.insert(val.data.end(), data.begin(), data.end());
        return *this;
    }

    template <typename T>
    PackStream &operator>>(T &val)
    {
        unsigned char *packed = new unsigned char[CALC_SIZE(T)];
        for (int i = 0; i < CALC_SIZE(T); i++)
        {
            packed[i] = data[i];
            data.erase(data.begin(), data.begin() + 1);
        }
        auto *res = reinterpret_cast<T *>(packed);
        val = *res;
        return *this;
    }

    template <StreamObjType T>
    PackStream &operator>>(T &val)
    {
        val.read(*this);
        return *this;
    }

#define GET_INT(num_type)                                  \
    auto _size = CALC_SIZE(num_type);                      \
    unsigned char *packed = new unsigned char[_size];      \
    for (int i = 0; i < _size; i++)                        \
    {                                                      \
        packed[i] = data[i];                               \
    }                                                      \
    num_type *val2 = reinterpret_cast<num_type *>(packed); \
    val = *val2;                                           \
    data.erase(data.begin(), data.begin() + _size);        \
    delete packed;                                         \
    return *this;

    template <StreamIntType T>
    PackStream &operator>>(T &val)
    {
        unsigned char code = data.front();
        data.erase(data.begin(), data.begin() + 1);
        if (code < 0xfd)
        {
            val = code;
        }
        else if (code == 0xfd)
        {
            GET_INT(uint16_t)
        }
        else if (code == 0xfe)
        {
            GET_INT(uint32_t)
        }
        else if (code == 0xff)
        {
            GET_INT(uint64_t)
        }

        return *this;
        //throw std::invalid_argument();
    }

    unsigned char *bytes() const
    {
        unsigned char *result = new unsigned char[data.size()];
        std::copy(data.begin(), data.end(), result);
        return result;
    }

    size_t size() const
    {
        return data.size();
    }

    bool isNull() const
    {
        return data.size() > 0;
    }
};

#undef GET_INT