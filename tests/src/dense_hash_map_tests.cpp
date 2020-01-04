#define CATCH_CONFIG_MAIN // This tells Catch to provide a main() - only do this in one cpp file

#include "catch2/catch.hpp"
#include "jg/dense_hash_map.hpp"
#include "jg/details/type_traits.hpp"

#include <string>
#include <vector>

namespace
{
struct increase_counter_on_copy_or_move
{
    increase_counter_on_copy_or_move(std::size_t* counter_ptr) : counter_ptr{counter_ptr} {}

    increase_counter_on_copy_or_move(const increase_counter_on_copy_or_move& other)
        : counter_ptr(other.counter_ptr)
    {
        ++(*counter_ptr);
    }

    increase_counter_on_copy_or_move(increase_counter_on_copy_or_move&& other)
        : counter_ptr(std::move(other.counter_ptr))
    {
        ++(*counter_ptr);
    }

    auto operator=(const increase_counter_on_copy_or_move& other)
        -> increase_counter_on_copy_or_move&
    {
        counter_ptr = other.counter_ptr;
        ++(*counter_ptr);
        return *this;
    }

    auto operator=(increase_counter_on_copy_or_move&& other) -> increase_counter_on_copy_or_move&
    {
        counter_ptr = std::move(other.counter_ptr);
        ++(*counter_ptr);
        return *this;
    }

    std::size_t* counter_ptr = nullptr;
};

auto operator==(const increase_counter_on_copy_or_move&, const increase_counter_on_copy_or_move&)
    -> bool
{
    return true;
}

struct collision_hasher
{
    template <class T>
    auto operator()(T&&) const noexcept -> std::size_t
    {
        return 0;
    }
};

template <class T, class propagateSwap = std::false_type>
struct named_allocator
{
    using value_type = T;
    using propagate_on_container_swap = propagateSwap;

    named_allocator() = default;

    named_allocator(std::string name) : name(std::move(name)) {}

    template <class U, class propagateSwap2>
    constexpr named_allocator(const named_allocator<U, propagateSwap2>& other) noexcept
        : name(other.name)
    {}

    auto allocate(std::size_t n) -> T* { return std::allocator<T>{}.allocate(n); }

    void deallocate(T* p, std::size_t s) noexcept { std::allocator<T>{}.deallocate(p, s); }

    std::string name;
};

template <class T, class U, class propagateSwap>
auto operator==(
    const named_allocator<T, propagateSwap>& lhs, const named_allocator<U, propagateSwap>& rhs)
    -> bool
{
    return lhs.name == rhs.name;
}
template <class T, class U, class propagateSwap>
auto operator!=(
    const named_allocator<T, propagateSwap>& lhs, const named_allocator<U, propagateSwap>& rhs)
    -> bool
{
    return lhs.name != rhs.name;
}

struct nested_string 
{
    std::string value;
};

struct string_hash {
  using transparent_key_equal = std::equal_to<>; 
  using hash_type = std::hash<std::string>; 
  size_t operator()(const std::string& s) const { return hash_type{}(s); }
  size_t operator()(const nested_string& s) const { return hash_type{}(s.value); }
};

bool operator==(const std::string& s, const nested_string& ns)
{
    return s == ns.value;
}

template <typename UnnamedType>
struct is_valid_container
{
private:
    template <typename... Params> constexpr auto test_validity(int /* unused */)
        -> decltype(std::declval<UnnamedType>()(std::declval<Params>()...), std::true_type())
    {
        return std::true_type();
    }

    template <typename... Params> constexpr std::false_type test_validity(...)
    {
        return std::false_type();
    }

public:
    template <typename... Params> constexpr auto operator()(Params&& ...)
    {
        return test_validity<Params...>(int());
    }
};

template <typename UnnamedType>
constexpr auto is_valid(UnnamedType&&)
{
    return is_valid_container<UnnamedType>();
}

} // namespace

namespace std
{
template <>
struct hash<increase_counter_on_copy_or_move>
{
    auto operator()(const increase_counter_on_copy_or_move&) const noexcept -> std::size_t
    {
        return 0;
    }
};
} // namespace std

TEST_CASE("clear")
{
    jg::dense_hash_map<std::string, int> m;

    SECTION("empty")
    {
        m.clear();
        REQUIRE(m.empty());
        REQUIRE(m.size() == 0);
        REQUIRE(m.bucket_count() == 8u);
        REQUIRE(m.load_factor() == 0.0f);
    }

    SECTION("not_empty")
    {
        m.try_emplace("sponge bob", 10);
        m.try_emplace("sponge bob2", 10);
        REQUIRE(m.size() == 2);

        m.clear();
        REQUIRE(m.empty());
        REQUIRE(m.size() == 0);
        REQUIRE(m.bucket_count() == 8u);
        REQUIRE(m.load_factor() == 0.0f);
    }

    SECTION("no_except") { REQUIRE(noexcept(m.clear())); }
}

template <class T, class V>
using has_insert = decltype(std::declval<T>().insert(std::declval<V>()));

template <class T, class V>
using has_insert_hint =
    decltype(std::declval<T>().insert(std::declval<T>().begin(), std::declval<V>()));

TEST_CASE("insert")
{
    jg::dense_hash_map<std::string, int> m;

    SECTION("insert - lvalue")
    {
        auto pair = std::pair(std::string("test"), 42);
        const auto& [it, result] = m.insert(pair);
        REQUIRE(result);
        REQUIRE(it->first == "test");
        REQUIRE(it->second == 42);
        REQUIRE(m.size() == 1);
    }

    SECTION("insert - const lvalue")
    {
        const auto pair = std::pair(std::string("test"), 42);
        const auto& [it, result] = m.insert(pair);
        REQUIRE(result);
        REQUIRE(it->first == "test");
        REQUIRE(it->second == 42);
        REQUIRE(m.size() == 1);
    }

    SECTION("insert - rvalue")
    {
        const auto& [it, result] = m.insert(std::pair(std::string("test"), 42));
        REQUIRE(result);
        REQUIRE(it->first == "test");
        REQUIRE(it->second == 42);
        REQUIRE(m.size() == 1);
    }

    SECTION("insert - conversion")
    {
        auto pair = std::pair("test", 42);
        const auto& [it, result] = m.insert(pair);
        REQUIRE(result);
        REQUIRE(it->first == "test");
        REQUIRE(it->second == 42);
        REQUIRE(m.size() == 1);
    }

    SECTION("insert - conversion is SFINAE friendly")
    {
        REQUIRE_FALSE(jg::details::is_detected<
                      has_insert, jg::dense_hash_map<std::vector<int>, int>,
                      std::pair<bool, std::string>>::value);
    }

    SECTION("insert - lvalue - hint")
    {
        auto pair = std::pair(std::string("test"), 42);
        const auto& it = m.insert(m.begin(), pair);
        REQUIRE(it != m.end());
        REQUIRE(it->first == "test");
        REQUIRE(it->second == 42);
        REQUIRE(m.size() == 1);
    }

    SECTION("insert - const lvalue - hint")
    {
        const auto pair = std::pair(std::string("test"), 42);
        const auto& it = m.insert(m.begin(), pair);
        REQUIRE(it != m.end());
        REQUIRE(it->first == "test");
        REQUIRE(it->second == 42);
        REQUIRE(m.size() == 1);
    }

    SECTION("insert - rvalue - hint")
    {
        const auto& it = m.insert(m.begin(), std::pair(std::string("test"), 42));
        REQUIRE(it != m.end());
        REQUIRE(it->first == "test");
        REQUIRE(it->second == 42);
        REQUIRE(m.size() == 1);
    }

    SECTION("insert - conversion - hint")
    {
        auto pair = std::pair("test", 42);
        const auto& it = m.insert(m.begin(), pair);
        REQUIRE(it != m.end());
        REQUIRE(it->first == "test");
        REQUIRE(it->second == 42);
        REQUIRE(m.size() == 1);
    }

    SECTION("insert - conversion is SFINAE friendly - hint")
    {
        REQUIRE_FALSE(jg::details::is_detected<
                      has_insert_hint, jg::dense_hash_map<std::vector<int>, int>,
                      std::pair<bool, std::string>>::value);
    }

    SECTION("insert - iterator")
    {
        std::vector<std::pair<std::string, int>> v{{"test", 42}, {"test2", 1337}};
        m.insert(v.begin(), v.end());

        REQUIRE(m.size() == 2);

        auto it = m.find("test");
        REQUIRE(it != m.end());
        REQUIRE(it->second == 42);

        it = m.find("test2");
        REQUIRE(it != m.end());
        REQUIRE(it->second == 1337);
    }

    SECTION("insert - initializer_list")
    {
        std::initializer_list<std::pair<const std::string, int>> l{{"test", 42}, {"test2", 1337}};
        m.insert(l);

        REQUIRE(m.size() == 2);

        auto it = m.find("test");
        REQUIRE(it != m.end());
        REQUIRE(it->second == 42);

        it = m.find("test2");
        REQUIRE(it != m.end());
        REQUIRE(it->second == 1337);
    }
}

TEST_CASE("insert_or_assign")
{
    jg::dense_hash_map<std::string, int> m1;
    jg::dense_hash_map<std::unique_ptr<int>, int> m2;

    SECTION("l-value key")
    {
        const auto& [it, result] = m1.insert_or_assign("test", 42);
        REQUIRE(result);
        REQUIRE(it->first == "test");
        REQUIRE(it->second == 42);

        const auto& [it2, result2] = m1.insert_or_assign("test", 1337);
        REQUIRE_FALSE(result2);
        REQUIRE(it2 == it);
        REQUIRE(it2->first == "test");
        REQUIRE(it2->second == 1337);
    }

    SECTION("r-value key")
    {
        const auto& [it, result] = m2.insert_or_assign(nullptr, 42);
        REQUIRE(result);
        REQUIRE(it->first == nullptr);
        REQUIRE(it->second == 42);

        std::unique_ptr<int> p = nullptr;
        const auto& [it2, result2] = m2.insert_or_assign(std::move(p), 1337);
        REQUIRE_FALSE(result2);
        REQUIRE(it2 == it);
        REQUIRE(it2->first == nullptr);
        REQUIRE(it2->second == 1337);
    }

    SECTION("l-value key - hint")
    {
        auto it = m1.insert_or_assign(m1.begin(), "test", 42);
        REQUIRE(it->first == "test");
        REQUIRE(it->second == 42);

        auto it2 = m1.insert_or_assign(m1.begin(), "test", 1337);
        REQUIRE(it == it);
        REQUIRE(it2->first == "test");
        REQUIRE(it2->second == 1337);
    }

    SECTION("r-value key - hint")
    {
        auto it = m2.insert_or_assign(m2.begin(), nullptr, 42);
        REQUIRE(it->first == nullptr);
        REQUIRE(it->second == 42);

        std::unique_ptr<int> p = nullptr;
        auto it2 = m2.insert_or_assign(m2.begin(), std::move(p), 1337);
        REQUIRE(it2 == it);
        REQUIRE(it2->first == nullptr);
        REQUIRE(it2->second == 1337);
    }
}

TEST_CASE("emplace", "[emplace]")
{
    jg::dense_hash_map<std::string, int> m;

    SECTION("default")
    {
        const auto& [it, result] = m.emplace();
        REQUIRE(result);
        REQUIRE(it->first == std::string());
        REQUIRE(it->second == int{});
        REQUIRE(m.size() == 1);
    }

    SECTION("once - rvalues")
    {
        const auto& [it, result] = m.emplace("test", 42);

        REQUIRE(result);
        REQUIRE(it->first == "test");
        REQUIRE(it->second == 42);
        REQUIRE(m.size() == 1);
    }

    SECTION("once - l-values")
    {
        std::string key = "test";
        int value = 42;
        const auto& [it, result] = m.emplace(key, value);

        REQUIRE(result);
        REQUIRE(it->first == "test");
        REQUIRE(it->second == 42);
        REQUIRE(m.size() == 1);
    }

    SECTION("once - const l-values")
    {
        const std::string key = "test";
        const int value = 42;
        const auto& [it, result] = m.emplace(key, value);

        REQUIRE(result);
        REQUIRE(it->first == "test");
        REQUIRE(it->second == 42);
        REQUIRE(m.size() == 1);
    }

    SECTION("pair - lvalue")
    {
        auto pair = std::pair(std::string("test"), 42);
        const auto& [it, result] = m.emplace(pair);
        REQUIRE(result);
        REQUIRE(it->first == "test");
        REQUIRE(it->second == 42);
        REQUIRE(m.size() == 1);
    }

    SECTION("pair - const lvalue")
    {
        const auto pair = std::pair(std::string("test"), 42);
        const auto& [it, result] = m.emplace(pair);
        REQUIRE(result);
        REQUIRE(it->first == "test");
        REQUIRE(it->second == 42);
        REQUIRE(m.size() == 1);
    }

    SECTION("pair - rvalue")
    {
        const auto& [it, result] = m.emplace(std::pair(std::string("test"), 42));
        REQUIRE(result);
        REQUIRE(it->first == "test");
        REQUIRE(it->second == 42);
        REQUIRE(m.size() == 1);
    }

    SECTION("conversion")
    {
        const auto& [it, result] = m.emplace("test", 42); // key: const char* ==> std::string
        REQUIRE(result);
        REQUIRE(it->first == "test");
        REQUIRE(it->second == 42);
        REQUIRE(m.size() == 1);
    }

    SECTION("conversion pair")
    {
        const auto& [it, result] =
            m.emplace(std::pair("test", 42)); // key: const char* ==> std::string
        REQUIRE(result);
        REQUIRE(it->first == "test");
        REQUIRE(it->second == 42);
        REQUIRE(m.size() == 1);
    }

    SECTION("piecewise_construct")
    {
        const auto& [it, result] = m.emplace(
            std::piecewise_construct, std::forward_as_tuple("test"), std::forward_as_tuple(42));
        REQUIRE(result);
        REQUIRE(it->first == "test");
        REQUIRE(it->second == 42);
        REQUIRE(m.size() == 1);
    }
}

TEST_CASE("emplace key rvalue")
{
    jg::dense_hash_map<std::unique_ptr<int>, int> m;

    SECTION("Successfull emplace")
    {
        auto ptr = std::make_unique<int>(37);
        const auto& [it, result] = m.emplace(std::move(ptr), 42);
        REQUIRE(result);
        REQUIRE(*it->first == 37);
        REQUIRE(it->second == 42);
        REQUIRE(ptr == nullptr);
    }
}

TEST_CASE("emplace twice", "[emplace]")
{
    jg::dense_hash_map<std::string, int> m;

    SECTION("twice same")
    {
        const auto& [it, result] = m.emplace("test", 42);
        REQUIRE(result);
        REQUIRE(it->first == "test");
        REQUIRE(it->second == 42);

        const auto& [it2, result2] = m.emplace("test", 42);
        REQUIRE_FALSE(result2);
        REQUIRE(it == it2);
        REQUIRE(it2->first == "test");
        REQUIRE(it2->second == 42);
        REQUIRE(m.size() == 1);
    }

    SECTION("twice different")
    {
        const auto& [it, result] = m.emplace("test", 42);
        REQUIRE(result);
        REQUIRE(it->first == "test");
        REQUIRE(it->second == 42);

        const auto& [it2, result2] = m.emplace("test2", 1337);
        REQUIRE(result2);
        REQUIRE(it != it2);
        REQUIRE(it2->first == "test2");
        REQUIRE(it2->second == 1337);
        REQUIRE(m.size() == 2);
    }
}

TEST_CASE("emplace optimization", "[emplace]")
{
    std::size_t counter{};

    jg::dense_hash_map<increase_counter_on_copy_or_move, int> m;
    const auto& [it, result] = m.emplace(&counter, 42);

    REQUIRE(result);
    REQUIRE(counter > 0);

    auto counter_after_insertion = counter;

    SECTION("key not copied if not inserted")
    {
        increase_counter_on_copy_or_move key{&counter};
        m.emplace(key, 42);

        REQUIRE(counter_after_insertion == counter);
    }

    SECTION("pair's key copied moved if not inserted")
    {
        increase_counter_on_copy_or_move key{&counter};
        std::pair<increase_counter_on_copy_or_move&, int> p(key, 42);
        m.emplace(p);

        REQUIRE(counter_after_insertion == counter);
    }

    SECTION("key not moved if not inserted")
    {
        increase_counter_on_copy_or_move key{&counter};
        m.emplace(std::move(key), 42);

        REQUIRE(counter_after_insertion == counter);
    }

    SECTION("pair's key not moved if not inserted")
    {
        std::pair<increase_counter_on_copy_or_move, int> p(&counter, 42);
        m.emplace(std::move(p));

        REQUIRE(counter_after_insertion == counter);
    }
}

TEST_CASE("emplace_hint")
{
    jg::dense_hash_map<std::string, int> m;

    SECTION("one")
    {
        const auto& it = m.emplace_hint(m.begin(), "bob", 666);
        REQUIRE(it != m.end());
        REQUIRE(it->first == "bob");
        REQUIRE(it->second == 666);
        REQUIRE(m.size() == 1);
    }

    SECTION("twice")
    {
        const auto& it1 = m.emplace_hint(m.begin(), "bob", 666);
        const auto& it2 = m.emplace_hint(m.begin(), "bob", 444);
        REQUIRE(it1 == it2);
        REQUIRE(it2->first == "bob");
        REQUIRE(it2->second == 666);
        REQUIRE(m.size() == 1);
    }
}

TEST_CASE("try emplace", "[try_emplace]")
{
    jg::dense_hash_map<std::string, int> m;

    SECTION("rvalues")
    {
        const auto& [it, result] = m.try_emplace("test", 42);

        REQUIRE(result);
        REQUIRE(it->first == "test");
        REQUIRE(it->second == 42);
        REQUIRE(m.size() == 1);
    }

    SECTION("l-values")
    {
        std::string key = "test";
        int value = 42;
        const auto& [it, result] = m.try_emplace(key, value);

        REQUIRE(result);
        REQUIRE(it->first == "test");
        REQUIRE(it->second == 42);
        REQUIRE(m.size() == 1);
    }

    SECTION("const l-values")
    {
        const std::string key = "test";
        const int value = 42;
        const auto& [it, result] = m.try_emplace(key, value);

        REQUIRE(result);
        REQUIRE(it->first == "test");
        REQUIRE(it->second == 42);
        REQUIRE(m.size() == 1);
    }
}

TEST_CASE("try_emplace twice", "[try_emplace]")
{
    jg::dense_hash_map<std::string, int> m;

    SECTION("twice same")
    {
        const auto& [it, result] = m.try_emplace("test", 42);
        REQUIRE(result);
        REQUIRE(it->first == "test");
        REQUIRE(it->second == 42);

        const auto& [it2, result2] = m.try_emplace("test", 42);
        REQUIRE_FALSE(result2);
        REQUIRE(it == it2);
        REQUIRE(it2->first == "test");
        REQUIRE(it2->second == 42);
        REQUIRE(m.size() == 1);
    }

    SECTION("twice different")
    {
        const auto& [it, result] = m.try_emplace("test", 42);
        REQUIRE(result);
        REQUIRE(it->first == "test");
        REQUIRE(it->second == 42);

        const auto& [it2, result2] = m.try_emplace("test2", 1337);
        REQUIRE(result2);
        REQUIRE(it != it2);
        REQUIRE(it2->first == "test2");
        REQUIRE(it2->second == 1337);
        REQUIRE(m.size() == 2);
    }
}

TEST_CASE("try_emplace effects guarantees", "[try_emplace]")
{
    std::size_t counter{};

    jg::dense_hash_map<increase_counter_on_copy_or_move, int> m;
    const auto& [it, result] = m.emplace(&counter, 42);

    REQUIRE(result);
    REQUIRE(counter > 0);

    auto counter_after_insertion = counter;

    SECTION("key not copied if not inserted")
    {
        increase_counter_on_copy_or_move key{&counter};
        m.try_emplace(key, 42);

        REQUIRE(counter_after_insertion == counter);
    }

    SECTION("key not moved if not inserted")
    {
        increase_counter_on_copy_or_move key{&counter};
        m.try_emplace(std::move(key), 42);

        REQUIRE(counter_after_insertion == counter);
    }
}

TEST_CASE("erase iterator", "[erase]")
{
    jg::dense_hash_map<std::string, int> m;
    const auto& [useless1, result1] = m.emplace("bob", 42);
    const auto& [useless2, result2] = m.emplace("jacky", 42);
    const auto& [useless3, result3] = m.emplace("snoop", 42);

    REQUIRE(result1);
    REQUIRE(result2);
    REQUIRE(result3);
    REQUIRE(m.find("bob") != m.end());
    REQUIRE(m.find("jacky") != m.end());
    REQUIRE(m.find("snoop") != m.end());
    REQUIRE(m.size() == 3);

    SECTION("using first iterator")
    {
        auto new_it = m.erase(m.begin());
        REQUIRE(new_it != m.end());
        REQUIRE(m.size() == 2);
        REQUIRE(new_it->first == "snoop");
        REQUIRE(m.find("bob") == m.end());
        REQUIRE(m.find("jacky") != m.end());
        REQUIRE(m.find("snoop") != m.end());
    }

    SECTION("using middle iterator")
    {
        auto it = m.find("jacky");
        REQUIRE(it != m.end());
        auto new_it = m.erase(it);
        REQUIRE(new_it != m.end());
        REQUIRE(m.size() == 2);
        REQUIRE(new_it->first == "snoop");
        REQUIRE(m.find("bob") != m.end());
        REQUIRE(m.find("jacky") == m.end());
        REQUIRE(m.find("snoop") != m.end());
    }

    SECTION("using last iterator")
    {
        auto new_it = m.erase(std::prev(m.end()));
        REQUIRE(new_it == m.end());
        REQUIRE(m.size() == 2);
        REQUIRE(m.find("bob") != m.end());
        REQUIRE(m.find("jacky") != m.end());
        REQUIRE(m.find("snoop") == m.end());
    }
}

TEST_CASE("erase key", "[erase]")
{
    jg::dense_hash_map<std::string, int> m;
    const auto& [useless1, result1] = m.emplace("bob", 42);
    const auto& [useless2, result2] = m.emplace("jacky", 42);
    const auto& [useless3, result3] = m.emplace("snoop", 42);

    REQUIRE(result1);
    REQUIRE(result2);
    REQUIRE(result3);
    REQUIRE(m.find("bob") != m.end());
    REQUIRE(m.find("jacky") != m.end());
    REQUIRE(m.find("snoop") != m.end());
    REQUIRE(m.size() == 3);

    SECTION("success")
    {
        REQUIRE(m.erase("bob") > 0);
        REQUIRE(m.size() == 2);
        REQUIRE(m.find("bob") == m.end());
        REQUIRE(m.find("jacky") != m.end());
        REQUIRE(m.find("snoop") != m.end());
    }

    SECTION("failure")
    {
        REQUIRE(m.erase("bobby") == 0);
        REQUIRE(m.size() == 3);
        REQUIRE(m.find("bob") != m.end());
        REQUIRE(m.find("jacky") != m.end());
        REQUIRE(m.find("snoop") != m.end());
    }
}

TEST_CASE("erase with collisions", "[erase]")
{
    jg::dense_hash_map<std::string, int, collision_hasher> m;

    const auto& [useless1, result1] = m.emplace("bob", 42);
    const auto& [useless2, result2] = m.emplace("jacky", 42);
    const auto& [useless3, result3] = m.emplace("snoop", 42);

    REQUIRE(result1);
    REQUIRE(result2);
    REQUIRE(result3);
    auto bob_it = m.find("bob");
    auto jacky_it = m.find("jacky");
    auto snoop_it = m.find("snoop");
    REQUIRE(bob_it != m.end());
    REQUIRE(jacky_it != m.end());
    REQUIRE(snoop_it != m.end());
    REQUIRE(m.size() == 3);

    SECTION("remove first in bucket")
    {
        REQUIRE(m.erase("snoop") > 0);
        REQUIRE(m.size() == 2);
        REQUIRE(m.find("bob") != m.end());
        REQUIRE(m.find("jacky") != m.end());
        REQUIRE(m.find("snoop") == m.end());
    }

    SECTION("remove mid in bucket")
    {
        REQUIRE(m.erase("jacky") > 0);
        REQUIRE(m.size() == 2);
        REQUIRE(m.find("bob") != m.end());
        REQUIRE(m.find("jacky") == m.end());
        REQUIRE(m.find("snoop") != m.end());
    }

    SECTION("remove last in bucket")
    {
        REQUIRE(m.erase("bob") > 0);
        REQUIRE(m.size() == 2);
        REQUIRE(m.find("bob") == m.end());
        REQUIRE(m.find("jacky") != m.end());
        REQUIRE(m.find("snoop") != m.end());
    }
}

TEST_CASE("range erase")
{
    jg::dense_hash_map<std::string, int> m;
    const auto& [useless1, result1] = m.emplace("bob", 42);
    const auto& [useless2, result2] = m.emplace("jacky", 42);
    const auto& [useless3, result3] = m.emplace("snoop", 42);

    REQUIRE(result1);
    REQUIRE(result2);
    REQUIRE(result3);
    REQUIRE(m.find("bob") != m.end());
    REQUIRE(m.find("jacky") != m.end());
    REQUIRE(m.find("snoop") != m.end());
    REQUIRE(m.size() == 3);

    SECTION("all")
    {
        auto it = m.erase(m.begin(), m.end());
        REQUIRE(it == m.end());
        REQUIRE(m.size() == 0);
        REQUIRE(m.find("bob") == m.end());
        REQUIRE(m.find("jacky") == m.end());
        REQUIRE(m.find("snoop") == m.end());
    }

    SECTION("two first")
    {
        auto it = m.erase(m.begin(), std::prev(m.end()));
        REQUIRE(it->first == "snoop");
        REQUIRE(m.size() == 1);
        REQUIRE(m.find("bob") == m.end());
        REQUIRE(m.find("jacky") == m.end());
        REQUIRE(m.find("snoop") != m.end());
    }

    SECTION("two last")
    {
        auto it = m.erase(std::next(m.begin()), m.end());
        REQUIRE(it == m.end());
        REQUIRE(m.size() == 1);
        REQUIRE(m.find("bob") != m.end());
        REQUIRE(m.find("jacky") == m.end());
        REQUIRE(m.find("snoop") == m.end());
    }

    SECTION("none")
    {
        auto it = m.erase(m.begin(), m.begin());
        REQUIRE(it == m.begin());
        REQUIRE(m.size() == 3);
        REQUIRE(m.find("bob") != m.end());
        REQUIRE(m.find("jacky") != m.end());
        REQUIRE(m.find("snoop") != m.end());
    }
}

TEST_CASE("rehash")
{
    jg::dense_hash_map<std::string, int> m;

    SECTION("by insertion")
    {
        const int amount = 1000;
        const auto old_bucket_count = m.bucket_count();

        for (int i = 0; i < amount; ++i)
        {
            m.emplace("test" + std::to_string(i), i);
        }

        REQUIRE(m.size() == amount);
        REQUIRE(m.bucket_count() != old_bucket_count);
        REQUIRE(m.bucket_count() >= std::ceil(1000 / m.max_load_factor()));

        for (int i = 0; i < amount; ++i)
        {
            const std::string key = "test" + std::to_string(i);
            auto it = m.find(key);
            REQUIRE(it != m.end());
            REQUIRE(it->first == key);
            REQUIRE(it->second == i);
        }
    }

    SECTION("rehash member function")
    {
        m.try_emplace("tarzan", 42);
        m.try_emplace("spirou", 1337);

        const auto old_load_factor = m.load_factor();

        m.rehash(1000);

        REQUIRE(m.load_factor() < old_load_factor);
        REQUIRE(m.bucket_count() == 1024);

        auto it = m.find("tarzan");
        REQUIRE(it != m.end());
        REQUIRE(it->first == "tarzan");
        REQUIRE(it->second == 42);

        it = m.find("spirou");
        REQUIRE(it != m.end());
        REQUIRE(it->first == "spirou");
        REQUIRE(it->second == 1337);
    }

    SECTION("reserve")
    {
        m.try_emplace("tarzan", 42);
        m.try_emplace("spirou", 1337);

        const auto old_load_factor = m.load_factor();

        m.reserve(1000);

        REQUIRE(m.load_factor() < old_load_factor);
        REQUIRE(m.bucket_count() >= std::ceil(1000 / m.max_load_factor()));

        auto it = m.find("tarzan");
        REQUIRE(it != m.end());
        REQUIRE(it->first == "tarzan");
        REQUIRE(it->second == 42);

        it = m.find("spirou");
        REQUIRE(it != m.end());
        REQUIRE(it->first == "spirou");
        REQUIRE(it->second == 1337);
    }

    SECTION("max_load_factor")
    {
        m.try_emplace("tarzan", 42);
        m.try_emplace("spirou", 1337);

        REQUIRE(m.bucket_count() == 8);
        REQUIRE(m.load_factor() == 0.25); // We have 2/8 == 0.25 load factor.

        m.max_load_factor(0.2f); // So this should trigger a regrowth.

        REQUIRE(m.max_load_factor() == 0.2f);
        REQUIRE(m.bucket_count() == 16); // Doubling the size will work.

        auto it = m.find("tarzan");
        REQUIRE(it != m.end());
        REQUIRE(it->first == "tarzan");
        REQUIRE(it->second == 42);

        it = m.find("spirou");
        REQUIRE(it != m.end());
        REQUIRE(it->first == "spirou");
        REQUIRE(it->second == 1337);
    }
}

TEST_CASE("swap", "[swap]")
{
    jg::dense_hash_map<std::string, int> m1 = {{"batman", 42}, {"robin", 666}};
    jg::dense_hash_map<std::string, int> m2 = {{"superman", 64}};

    SECTION("member")
    {
        m1.swap(m2);

        REQUIRE(m1.size() == 1);
        REQUIRE(m2.size() == 2);

        auto it = m1.find("superman");
        REQUIRE(it != m1.end());
        REQUIRE(it->first == "superman");
        REQUIRE(it->second == 64);
        it = m1.find("batman");
        REQUIRE(it == m1.end());

        it = m2.find("robin");
        REQUIRE(it != m2.end());
        REQUIRE(it->first == "robin");
        REQUIRE(it->second == 666);
    }

    SECTION("std::swap")
    {
        std::swap(m1, m2);

        REQUIRE(m1.size() == 1);
        REQUIRE(m2.size() == 2);

        auto it = m1.find("superman");
        REQUIRE(it != m1.end());
        REQUIRE(it->first == "superman");
        REQUIRE(it->second == 64);
        it = m1.find("batman");
        REQUIRE(it == m1.end());

        it = m2.find("robin");
        REQUIRE(it != m2.end());
        REQUIRE(it->first == "robin");
        REQUIRE(it->second == 666);
    }
}

TEST_CASE("swap allocator", "[swap]")
{
    SECTION("no swap")
    {
        using alloc = named_allocator<int>;
        jg::dense_hash_map<int, int, std::hash<int>, std::equal_to<int>, alloc> m1{alloc{"a1"}};
        jg::dense_hash_map<int, int, std::hash<int>, std::equal_to<int>, alloc> m2{alloc{"a2"}};

        REQUIRE(m1.get_allocator().name == "a1");
        REQUIRE(m2.get_allocator().name == "a2");

        std::swap(m1, m2);

        REQUIRE(m1.get_allocator().name == "a1");
        REQUIRE(m2.get_allocator().name == "a2");
    }

    SECTION("swap")
    {
        using alloc = named_allocator<int, std::true_type>;
        jg::dense_hash_map<int, int, std::hash<int>, std::equal_to<int>, alloc> m1{alloc{"a1"}};
        jg::dense_hash_map<int, int, std::hash<int>, std::equal_to<int>, alloc> m2{alloc{"a2"}};

        REQUIRE(m1.get_allocator().name == "a1");
        REQUIRE(m2.get_allocator().name == "a2");

        std::swap(m1, m2);

        REQUIRE(m1.get_allocator().name == "a2");
        REQUIRE(m2.get_allocator().name == "a1");
    }
}

TEST_CASE("at")
{
    jg::dense_hash_map<std::string, int> m1 = {{"batman", 42}, {"robin", 666}};

    // TODO: exception free.
    SECTION("non-const")
    {
        static_assert(std::is_same_v<decltype(m1.at("")), int&>);
        REQUIRE(m1.at("batman") == 42);

        m1.at("batman") = 1337;

        REQUIRE(m1.at("batman") == 1337);

        REQUIRE_THROWS(m1.at("winnie"));
    }

    SECTION("const")
    {
        const auto& m2 = m1;
        static_assert(std::is_same_v<decltype(m2.at("")), const int&>);
        REQUIRE(m2.at("batman") == 42);
        REQUIRE_THROWS(m1.at("the pooh"));
    }
}

TEST_CASE("subscript operator")
{
    jg::dense_hash_map<std::string, int> m1 = {};

    SECTION("copy-key")
    {
        std::string key = "RPC";
        m1[key] = 4;
        REQUIRE(m1.size() == 1);
        auto it = m1.find("RPC");
        REQUIRE(it != m1.end());
        REQUIRE(it->second == 4);
    }

    SECTION("move-key")
    {
        std::string key = "PRC";
        m1[std::move(key)] = 748;
        REQUIRE(m1.size() == 1);
        auto it = m1.find("PRC");
        REQUIRE(it != m1.end());
        REQUIRE(it->second == 748);
        REQUIRE(key.empty());
    }

    SECTION("twice")
    {
        m1["CRP"] = 1;
        REQUIRE(m1.size() == 1);
        auto it = m1.find("CRP");
        REQUIRE(it != m1.end());
        REQUIRE(it->second == 1);

        m1["CRP"] = 2;
        REQUIRE(it->second == 2);
    }
}

TEST_CASE("count simple hash", "[count]")
{
    jg::dense_hash_map<std::string, int> m1 = {};

    SECTION("key")
    {
        REQUIRE(m1.count("queen") == 0);

        m1.try_emplace("queen", 42);
        REQUIRE(m1.count("queen") == 1);
    }

    SECTION("no generic count")
    {
        auto v = is_valid([](const auto& m) -> decltype(m.count(nested_string{"queen"})) {});
        REQUIRE_FALSE(v(m1));
    }
}

TEST_CASE("count transparent hash", "[count]")
{
    jg::dense_hash_map<std::string, int, string_hash> m1 = {};

    SECTION("key")
    {
        REQUIRE(m1.count(nested_string{"pink floyd"}) == 0);

        m1.try_emplace("pink floyd", 42);
        REQUIRE(m1.count(nested_string{"pink floyd"}) == 1);
    }
}

TEST_CASE("find simple hash", "[find]")
{
    jg::dense_hash_map<std::string, int> m1 = {};

    SECTION("non-const")
    {
        auto it = m1.find("queen");
        REQUIRE(it == m1.end());

        m1.try_emplace("queen", 42);
        it = m1.find("queen");
        REQUIRE(it != m1.end());
        REQUIRE(it->second == 42);
        static_assert(std::is_same_v<decltype((it->second)), int&>);
    }

    SECTION("const")
    {   
        const auto& m2 = m1; 
        auto it = m2.find("queen");
        REQUIRE(it == m2.end());

        m1.try_emplace("queen", 42);
        it = m2.find("queen");
        REQUIRE(it != m2.end());
        REQUIRE(it->second == 42);
        static_assert(std::is_same_v<decltype((it->second)), const int&>);
    }

    SECTION("no generic find")
    {
        auto v = is_valid([](const auto& m) -> decltype(m.find(nested_string{"queen"})) {});
        REQUIRE_FALSE(v(m1));
    }
}

TEST_CASE("find transparent hash", "[find]")
{
    jg::dense_hash_map<std::string, int, string_hash> m1 = {};

    SECTION("non-const")
    {
        auto it = m1.find(nested_string{"queen"});
        REQUIRE(it == m1.end());

        m1.try_emplace("queen", 42);
        it = m1.find(nested_string{"queen"});
        REQUIRE(it != m1.end());
        REQUIRE(it->second == 42);
        static_assert(std::is_same_v<decltype((it->second)), int&>);
    }

    SECTION("const")
    {   
        const auto& m2 = m1; 
        auto it = m2.find(nested_string{"queen"});
        REQUIRE(it == m2.end());

        m1.try_emplace("queen", 42);
        it = m2.find(nested_string{"queen"});
        REQUIRE(it != m2.end());
        REQUIRE(it->second == 42);
        static_assert(std::is_same_v<decltype((it->second)), const int&>);
    }
}

TEST_CASE("contains simple hash", "[contains]")
{
    jg::dense_hash_map<std::string, int> m1 = {};

    SECTION("key")
    {
        REQUIRE_FALSE(m1.contains("queen"));

        m1.try_emplace("queen", 42);
        REQUIRE(m1.contains("queen"));
    }

    SECTION("no generic contains")
    {
        auto v = is_valid([](const auto& m) -> decltype(m.contains(nested_string{"queen"})) {});
        REQUIRE_FALSE(v(m1));
    }
}

TEST_CASE("contains transparent hash", "[contains]")
{
    jg::dense_hash_map<std::string, int, string_hash> m1 = {};

    SECTION("key")
    {
        REQUIRE_FALSE(m1.contains(nested_string{"pink floyd"}));

        m1.try_emplace("pink floyd", 42);
        REQUIRE(m1.contains(nested_string{"pink floyd"}));
    }
}

TEST_CASE("equal_range simple hash", "[equal_range]")
{
    jg::dense_hash_map<std::string, int> m1 = {};

    SECTION("non-const")
    {
        auto [it, end] = m1.equal_range("queen");
        REQUIRE(it == m1.end());
        REQUIRE(end == m1.end());

        m1.try_emplace("queen", 42);
        auto [it2, end2] = m1.equal_range("queen");
        REQUIRE(it2 != m1.end());
        REQUIRE(it2->second == 42);
        REQUIRE(std::distance(it2, end2) == 1);
        static_assert(std::is_same_v<decltype((it2->second)), int&>);
    }

    SECTION("const")
    {   
        const auto& m2 = m1; 
        auto [it, end] = m2.equal_range("queen");
        REQUIRE(it == m2.end());
        REQUIRE(end == m1.end());

        m1.try_emplace("queen", 42);
        auto [it2, end2] = m2.equal_range("queen");
        REQUIRE(it2 != m2.end());
        REQUIRE(it2->second == 42);
        REQUIRE(std::distance(it2, end2) == 1);
        static_assert(std::is_same_v<decltype((it2->second)), const int&>);
    }

    SECTION("no generic equal_range")
    {
        auto v = is_valid([](const auto& m) -> decltype(m.equal_range(nested_string{"queen"})) {});
        REQUIRE_FALSE(v(m1));
    }
}

TEST_CASE("equal_range transparent hash", "[equal_range]")
{
    jg::dense_hash_map<std::string, int, string_hash> m1 = {};

    SECTION("non-const")
    {
        auto [it, end] = m1.equal_range(nested_string{"queen"});
        REQUIRE(it == m1.end());
        REQUIRE(end == m1.end());

        m1.try_emplace("queen", 42);
        auto [it2, end2] = m1.equal_range(nested_string{"queen"});
        REQUIRE(it2 != m1.end());
        REQUIRE(it2->second == 42);
        REQUIRE(std::distance(it2, end2) == 1);
        static_assert(std::is_same_v<decltype((it2->second)), int&>);
    }

    SECTION("const")
    {   
        const auto& m2 = m1; 
        auto [it, end] = m2.equal_range(nested_string{"queen"});
        REQUIRE(it == m2.end());

        m1.try_emplace("queen", 42);
        auto [it2, end2] = m2.equal_range(nested_string{"queen"});
        REQUIRE(it2 != m2.end());
        REQUIRE(it2->second == 42);
        REQUIRE(std::distance(it2, end2) == 1);
        static_assert(std::is_same_v<decltype((it2->second)), const int&>);
    }
}

TEST_CASE("Move only types") {}

TEST_CASE("growth policy") {}
