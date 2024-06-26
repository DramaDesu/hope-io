/* Copyright (C) 2023 - 2024 Gleb Bezborodov - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the MIT license.
 *
 * You should have received a copy of the MIT license with
 * this file. If not, please write to: bezborodoff.gleb@gmail.com, or visit : https://github.com/glensand/daedalus-proto-lib
 */

#pragma once

#include "hope-io/proto/argument.h"

namespace hope::proto {

    template<typename TValue, e_argument_type Type>
    class argument_generic : public argument {
        constexpr static bool is_trivial = std::is_trivial_v<TValue> || std::is_same_v<std::string, TValue>;
    public:

        constexpr static e_argument_type type = Type;

        argument_generic()
            : argument(Type){}

        argument_generic(std::string&& in_name, TValue&& in_val)
                : argument(std::move(in_name), Type)
                , val(std::move(in_val)) {}

        argument_generic(std::string&& in_name, const TValue& in_val)
                : argument(std::move(in_name), Type)
                , val(in_val) {}

        [[nodiscard]] const TValue& get() const { return val; }

    protected:
        virtual void write_value(io::stream& stream) override {
            assert(is_trivial);
            if constexpr(is_trivial) {
                stream.write(val);
            }
        }

        virtual void read_value(io::stream& stream) override {
            assert(is_trivial);
            if constexpr(is_trivial) {
                return stream.read(val);
            }
        }

        [[nodiscard]] virtual void* get_value_internal() const override {
            return (void*)(&val);
        }

        TValue val;
    };

    using int32 = argument_generic<int32_t, e_argument_type::int32>;
    using uint64 = argument_generic<uint64_t, e_argument_type::uint64>;
    using float64 = argument_generic<double, e_argument_type::float64>;
    using string = argument_generic<std::string, e_argument_type::string>;
}
