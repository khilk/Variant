#include <variant.hpp>

#include <memory>
#include <string>

#include <gtest/gtest.h>

using util::bad_get;
using util::get;
using util::variant;

namespace {
struct helper {
  int tag;
  int* destructor_cnt;
  int* copy_cnt;
  bool throw_on_copy;

  helper(int tag, int* destructor_cnt, int* copy_cnt, bool throw_on_copy = false)
    : tag(tag), destructor_cnt(destructor_cnt), copy_cnt(copy_cnt), throw_on_copy(throw_on_copy) {
  }

  helper(const helper& other)
    : tag(other.tag),
      destructor_cnt(other.destructor_cnt),
      copy_cnt(other.copy_cnt),
      throw_on_copy(other.throw_on_copy) {
    (*copy_cnt)++;
    if (throw_on_copy)
      throw std::bad_alloc();
  }

  helper(helper&& other) = default;

  helper& operator=(helper const& other) = default;

  helper& operator=(helper&& other) = default;

  virtual ~helper() {
    (*destructor_cnt)++;
  }
};

struct A {};
struct B : A {};
}  // namespace

TEST(DefaultConstructor, Variant) {
  variant<int> a;
  EXPECT_THROW(get<int>(a), bad_get);

  variant<int, double, short> b;
  EXPECT_THROW(get<short>(b), bad_get);
}

TEST(ValueConstructor, Variant) {
  variant<int, double> a(5.0);
  EXPECT_EQ(get<double>(a), 5.0);
  EXPECT_THROW(get<int>(a), bad_get);

  variant<int, double> b(5);
  EXPECT_EQ(get<int>(b), 5);
  EXPECT_THROW(get<double>(b), bad_get);

  variant<int, std::string> c("Hello");
  EXPECT_EQ(get<std::string>(c), "Hello");
  EXPECT_THROW(get<int>(c), bad_get);
}

TEST(MoveSemantics, Variant) {
  std::unique_ptr<int> ptr(new int);

  variant<int, std::unique_ptr<int>> a(std::move(ptr));
  variant<int, std::unique_ptr<int>> b;
  b = std::move(a);
  EXPECT_TRUE((a.empty() || !get<std::unique_ptr<int>>(a)));
  EXPECT_TRUE(get<std::unique_ptr<int>>(b));

  std::unique_ptr<int> ptr2 = get<std::unique_ptr<int>>(std::move(b));
  EXPECT_TRUE(ptr2);
  EXPECT_TRUE((a.empty() || !get<std::unique_ptr<int>>(a)));
  EXPECT_TRUE((b.empty() || !get<std::unique_ptr<int>>(b)));
}

TEST(Get, Variant) {
  const variant<int, helper> a(3);
  EXPECT_EQ(get<int>(a), 3);

  int i = 5;
  const variant<int*> b(&i);
  EXPECT_EQ(*get<int*>(b), 5);

  variant<int, helper> ax(3);
  EXPECT_EQ(get<int>(ax), 3);

  variant<int*> bx(&i);
  EXPECT_EQ(*get<int*>(bx), 5);
}

TEST(GetByPointer, Variant) {
  variant<int, helper> a(3);
  EXPECT_EQ(*get<int>(&a), 3);
  EXPECT_EQ(get<helper>(&a), nullptr);

  variant<int, helper> const b(3);
  EXPECT_EQ(*get<int>(&b), 3);
  EXPECT_EQ(get<helper>(&b), nullptr);
}

TEST(GetHiearchy, Variant) {
  variant<A, B> x;
  x = B();
  EXPECT_THROW(get<A>(x), bad_get);
  EXPECT_NO_THROW(get<B>(x));
}

TEST(GetByIndex, Variant) {
  variant<int, std::string> a(3);
  EXPECT_EQ(get<0>(a), 3);
  const auto aint = a;
  a = "Hello";
  EXPECT_EQ(get<1>(a), "Hello");
  const auto astring = a;
  EXPECT_EQ(get<0>(aint), 3);
  EXPECT_EQ(get<1>(astring), "Hello");
}

TEST(GetByIndexByPointer, Variant) {
  variant<int, std::string> a(3);
  EXPECT_EQ(*get<0>(&a), 3);
  EXPECT_EQ(get<1>(&a), nullptr);
  const auto aint = a;
  a = "Hello";
  EXPECT_EQ(*get<1>(&a), "Hello");
  EXPECT_EQ(get<0>(&a), nullptr);
  const auto astring = a;
  EXPECT_EQ(*get<0>(&aint), 3);
  EXPECT_EQ(*get<1>(&astring), "Hello");
}

TEST(CheckOfAlignment, Variant) {
  struct alignas(128) X {};
  EXPECT_EQ(alignof(variant<char, X>), 128);
}

TEST(TestSwap, Variant) {
  using std::swap;
  variant<int, std::string> a(3);
  variant<int, std::string> b("Hello");
  EXPECT_EQ(get<std::string>(b), "Hello");
  EXPECT_EQ(get<int>(a), 3);
  swap(a, b);
  EXPECT_EQ(get<std::string>(a), "Hello");
  EXPECT_EQ(get<int>(b), 3);
}

TEST(CheckDestructors, Variant) {
  int destructor_count = 0;
  int copy_cnt = 0;
  helper* helper_ptr = new helper(5, &destructor_count, &copy_cnt);
  {
    variant<helper, int, double> a(helper(5, &destructor_count, &copy_cnt));
    variant<int, helper*, double> b(helper_ptr);
  }
  EXPECT_EQ(destructor_count, 2);
  delete helper_ptr;
}

TEST(TestEmpty, Variant) {
  int destructor_count = 0;
  int copy_cnt = 0;
  variant<int, helper, double> b;
  EXPECT_TRUE(b.empty());
  b = helper(5, &destructor_count, &copy_cnt);
  EXPECT_TRUE(!b.empty());
}

TEST(TestClear, Variant) {
  int destructor_count = 0;
  int copy_cnt = 0;
  variant<int, helper, double> b(helper(5, &destructor_count, &copy_cnt));
  EXPECT_EQ(destructor_count, 1);  // Destroying temporary object
  EXPECT_TRUE(!b.empty());
  b.clear();
  EXPECT_TRUE(b.empty());
  EXPECT_EQ(destructor_count, 2);  // Destroying object on clear
}

TEST(TestIndex, Variant) {
  int destructor_count = 0;
  int copy_cnt = 0;
  variant<int, helper, std::string> b(helper(5, &destructor_count, &copy_cnt));
  EXPECT_EQ(b.index(), 1);
  b = 5;
  EXPECT_EQ(b.index(), 0);
  b = "Hello";
  EXPECT_EQ(b.index(), 2);
}

TEST(CopyConstructor, Variant) {
  int destructor_count = 0;
  int copy_cnt = 0;
  {
    variant<helper> a(helper(5, &destructor_count, &copy_cnt));
    variant<helper> b(a);
    EXPECT_EQ(get<helper>(a).tag, 5);
    EXPECT_EQ(get<helper>(b).tag, 5);
    EXPECT_GT(copy_cnt, 0);
  }
  EXPECT_EQ(destructor_count, 3);
}

TEST(CopyOperator, Variant) {
  variant<int, helper> a(3);
  variant<int, helper> b(5);
  b = a;
  EXPECT_EQ(get<int>(a), 3);
  EXPECT_EQ(get<int>(b), 3);
  b = variant<int, helper>(6);
  EXPECT_EQ(get<int>(b), 6);
}
