/* Copyright (C) 2023 - 2024 Gleb Bezborodov - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the MIT license.
 *
 * You should have received a copy of the MIT license with
 * this file. If not, please write to: bezborodoff.gleb@gmail.com, or visit : https://github.com/glensand/daedalus-proto-lib
 */
#pragma once
#pragma clang diagnostic push
#pragma ide diagnostic ignored "modernize-use-override"

#include "icarus-proto/protocol/argument_container.h"

namespace icarus::proto{

    class argument_struct final : public argument, public argument_container {
    public:
        constexpr static e_argument_type type = e_argument_type::struct_value;

        argument_struct()
            : argument(e_argument_type::struct_value){}

        virtual void write_value(io::stream& stream) override {
            write_values(stream);
        }

        virtual void read_value(io::stream& stream) override {
            read_values(stream);
        }

        template<typename T>
        const T& field(const std::string& name) {
            argument* arg{ nullptr };
            for (auto* candidate : values){
                if (candidate->get_name() == name){
                    arg = candidate;
                    break;
                }
            }
            return arg->as<T>();
        }

    private:
        argument_struct(std::string&& in_name, std::vector<argument*>&& args)
            : argument(std::move(in_name), e_argument_type::struct_value)
            , argument_container(std::move(args)){

        }

        [[nodiscard]] virtual void* get_value_internal() const override {
            return (void*)&values;
        }

        friend class struct_builder;
    };

    class struct_builder final {
    public:
        static struct_builder create() {
            return struct_builder{};
        }

        template<typename TValue, typename... Ts>
        struct_builder& add(Ts&&... args) {
            values.emplace_back(new TValue(std::forward<Ts>(args)...));
            return *this;
        }

        argument_struct* get(std::string&& name) {
            return new argument_struct(std::move(name), std::move(values));
        }

        virtual ~struct_builder(){
            assert(values.empty());
        }
    private:
        std::vector<argument*> values;
        struct_builder() = default;
    };

}
#pragma clang diagnostic pop