# C++20 vs C++23 vs C++26 完整详细对比

> 生成日期：2026-05-16 | C++26 已于 2026 年 3 月 28 日正式获批

---

## 目录

1. [版本概览与定位](#1-版本概览与定位)
2. [语言核心特性对比](#2-语言核心特性对比)
3. [标准库特性对比](#3-标准库特性对比)
4. [各版本重磅特性详解](#4-各版本重磅特性详解)
   - [4.1 C++20 四大支柱](#41-c20-四大支柱)
   - [4.2 C++23 关键补全](#42-c23-关键补全)
   - [4.3 C++26 四大突破](#43-c26-四大突破)
5. [编译器支持现状](#5-编译器支持现状)
6. [迁移建议](#6-迁移建议)

---

## 1. 版本概览与定位

| 维度 | C++20 | C++23 | C++26 |
|------|-------|-------|-------|
| **定位** | 革命性：自 C++11 以来最大变革 | 补全性：填补空白、修复缺陷、提升一致性 | 里程碑式：反射、安全、合约、异步四大突破 |
| **发布时间** | 2020 年 12 月 | 2023 年 2 月 | 2026 年 3 月（预计 2026 年内正式发布） |
| **ISO 编号** | ISO/IEC 14882:2020 | ISO/IEC 14882:2024 | ISO/IEC 14882:2026（待定） |
| **设计哲学** | 引入全新编程范式 | C++20 的"生产就绪版" | 面向安全的现代化 C++ |
| **核心主题** | Concepts、Ranges、Modules、Coroutines | 一致性、可用性、填补空白 | 反射、内存安全、契约、结构化异步 |
| **标准委员会主席评语** | — | — | Herb Sutter："自 C++11 以来最引人注目的版本" |

---

## 2. 语言核心特性对比

### 2.1 编译期计算

| 特性 | C++20 | C++23 | C++26 |
|------|:-----:|:-----:|:-----:|
| `constexpr` 基本支持 | ✅ | ✅ | ✅ |
| `consteval` 立即函数 | ✅ | ✅ | ✅ |
| `constinit` 强制编译期初始化 | ✅ | ✅ | ✅ |
| `constexpr` 虚函数 | ✅ | ✅ | ✅ |
| `constexpr` 中的 `try`/`catch` | ❌ | ✅ | ✅ |
| `constexpr` 中的 `dynamic_cast` / `typeid` | ❌ | ✅ | ✅ |
| `if consteval` 编译期分支 | ❌ | ✅ | ✅ |
| `constexpr` 函数中的 `goto`、`static` 变量、label | ❌ | ✅ | ✅ |
| `constexpr` 调用非 `constexpr` 虚函数（未被调用） | ❌ | ✅ | ✅ |
| `constexpr` placement new | ❌ | ❌ | ✅ |
| `constexpr` `void*` 转换 | ❌ | ❌ | ✅ |
| `constexpr` `std::vector` / `std::string`（完整支持） | ❌ | 部分 | ✅ |
| 编译期反射（`^^` 运算符 + `<meta>`） | ❌ | ❌ | ✅ |
| `constexpr std::format` | ❌ | ❌ | ✅ |

### 2.2 模板与元编程

| 特性 | C++20 | C++23 | C++26 |
|------|:-----:|:-----:|:-----:|
| Concepts（概念） | ✅ | ✅ | ✅ |
| 模板 Lambda `[]<typename T>(T v){}` | ✅ | ✅ | ✅ |
| 缩写函数模板（`auto` 参数） | ✅ | ✅ | ✅ |
| 非类型模板形参的类类型 | ✅ | ✅ | ✅ |
| 类模板参数推导（CTAD）聚合体支持 | ✅ | ✅ | ✅ |
| CTAD 从继承构造函数推导 | ❌ | ✅ | ✅ |
| 显式对象参数（deducing `this`） | ❌ | ✅ | ✅ |
| 包索引（Pack Indexing）`T...[0]` | ❌ | ❌ | ✅ |
| Variadic Friends | ❌ | ❌ | ✅ |
| `= delete("reason")` 带消息的删除函数 | ❌ | ❌ | ✅ |
| 显式对象参数在 lambda 中 | ❌ | ❌ | ✅（扩展） |

### 2.3 Lambda 表达式

| 特性 | C++20 | C++23 | C++26 |
|------|:-----:|:-----:|:-----:|
| 模板 Lambda | ✅ | ✅ | ✅ |
| `[=, this]` 显式捕获 | ✅ | ✅ | ✅ |
| Lambda 上的属性 | ❌ | ✅ | ✅ |
| `()` 在无参 Lambda 中更可选 | ❌ | ✅ | ✅ |
| 在未求值上下文中使用 Lambda | ❌ | ✅ | ✅ |
| 更简洁的 Lambda 推导（P1102） | ❌ | ✅ | ✅ |

### 2.4 控制流与语法

| 特性 | C++20 | C++23 | C++26 |
|------|:-----:|:-----:|:-----:|
| 三路比较运算符 `<=>` | ✅ | ✅ | ✅ |
| 范围 for 带初始化器 | ✅ | ✅ | ✅ |
| `if`/`switch` 带初始化语句 | ✅（C++17） | ✅ | ✅ |
| `[[likely]]` / `[[unlikely]]` | ✅ | ✅ | ✅ |
| `[[assume]]` 属性 | ❌ | ✅ | ✅ |
| `[[noreturn]]` 在 lambda 上 | ❌ | ✅ | ✅ |
| 指定初始化器（Designated Initializers） | ✅ | ✅ | ✅ |
| 使用 `using enum` | ✅ | ✅ | ✅ |
| `auto(x)` / `auto{x}` 衰减拷贝 | ❌ | ✅ | ✅ |
| 多维下标 `operator[]`（多个参数） | ❌ | ✅ | ✅ |
| `static operator()` / `static operator[]` | ❌ | ✅ | ✅ |
| 结构化绑定作为条件 | ❌ | ❌ | ✅ |
| 占位符变量 `_`（未命名变量） | ❌ | ❌ | ✅ |
| 合约：`pre` / `post` / `contract_assert` | ❌ | ❌ | ✅ |
| 未求值字符串 | ❌ | ❌ | ✅ |

### 2.5 预处理与源文件

| 特性 | C++20 | C++23 | C++26 |
|------|:-----:|:-----:|:-----:|
| `__VA_OPT__` | ✅ | ✅ | ✅ |
| `#elifdef` / `#elifndef` | ❌ | ✅ | ✅ |
| `#warning` | ❌ | ✅ | ✅ |
| `#embed` 二进制资源嵌入 | ❌ | ❌ | ✅ |
| UTF-8 可移植源文件编码 | ❌ | ✅ | ✅ |
| 命名通用字符转义 `\N{...}` | ❌ | ✅ | ✅ |
| 分隔转义序列 `\x{...}`（限制在 8 位） | ❌ | ✅ | ✅ |
| `@`、`$`、`` ` `` 字符保留供将来使用 | ❌ | ❌ | ✅ |

### 2.6 Modules（模块）

| 特性 | C++20 | C++23 | C++26 |
|------|:-----:|:-----:|:-----:|
| 模块基础（`export module` / `import`） | ✅ | ✅ | ✅ |
| `import std;` 标准库模块 | ❌ | ✅ | ✅ |
| 模块分区 | ✅ | ✅ | ✅ |
| 头文件单元（header units） | ✅ | ✅ | ✅ |

### 2.7 Coroutines（协程）

| 特性 | C++20 | C++23 | C++26 |
|------|:-----:|:-----:|:-----:|
| 协程基础（`co_await` / `co_yield` / `co_return`） | ✅ | ✅ | ✅ |
| `std::generator<T>` 标准协程生成器 | ❌ | ✅ | ✅ |
| 与 `std::execution` 框架集成 | ❌ | ❌ | ✅ |

### 2.8 内存安全

| 特性 | C++20 | C++23 | C++26 |
|------|:-----:|:-----:|:-----:|
| 未初始化读取 = 未定义行为（UB） | ✅ | ✅ | ❌（改为 Erroneous Behavior） |
| 标准库边界加固（`vector`、`span`、`string` 等） | ❌ | ❌ | ✅ |
| 模式匹配（Pattern Matching） | ❌ | ❌ | ✅（部分） |
| `std::is_within_lifetime` | ❌ | ❌ | ✅ |

---

## 3. 标准库特性对比

### 3.1 格式化与 I/O

| 特性 | C++20 | C++23 | C++26 |
|------|:-----:|:-----:|:-----:|
| `std::format` | ✅ | ✅ | ✅ |
| `std::print` / `std::println` | ❌ | ✅ | ✅ |
| 格式化 Range / 容器 / 元组 | ❌ | ✅ | ✅ |
| 格式化 `std::thread::id` / `std::stacktrace` | ❌ | ✅ | ✅ |
| `std::osyncstream`（线程安全输出） | ✅ | ✅ | ✅ |
| `std::spanstream`（基于 span 的流） | ❌ | ✅ | ✅ |
| `constexpr std::format` | ❌ | ❌ | ✅ |
| `std::formatter<std::filesystem::path>` | ❌ | ❌ | ✅ |
| `std::println()` 无参调用输出空行 | ❌ | ❌ | ✅ o |
| `std::stringstream` 从 `string_view` 构造 | ❌ | ❌ | ✅ |
| 文件流原生 OS 句柄 | ❌ | ❌ | ✅ |

### 3.2 Ranges（范围）

| 特性 | C++20 | C++23 | C++26 |
|------|:-----:|:-----:|:-----:|
| 基础 Ranges（`filter`、`transform`、`take` 等） | ✅ | ✅ | ✅ |
| `views::zip` | ❌ | ✅ | ✅ |
| `views::cartesian_product` | ❌ | ✅ | ✅ |
| `views::join_with` | ❌ | ✅ | ✅ |
| `views::chunk` / `views::slide` | ❌ | ✅ | ✅ |
| `views::chunk_by` | ❌ | ✅ | ✅ |
| `views::stride` | ❌ | ✅ | ✅ |
| `views::repeat` | ❌ | ✅ | ✅ |
| `views::enumerate` | ❌ | ✅ | ✅ |
| `views::as_rvalue` | ❌ | ✅ | ✅ |
| `views::as_const` | ❌ | ✅ | ✅ |
| `views::adjacent` / `views::adjacent_transform` | ❌ | ✅ | ✅ |
| `ranges::to<T>()` （Range 转容器） | ❌ | ✅ | ✅ |
| `ranges::contains()` / `ranges::contains_subrange()` | ❌ | ✅ | ✅ |
| `ranges::find_last()` 系列 | ❌ | ✅ | ✅ |
| `ranges::fold` 系列（`fold_left`、`fold_right` 等） | ❌ | ✅ | ✅ |
| `ranges::starts_with` / `ranges::ends_with` | ❌ | ✅ | ✅ |
| Range 适配器管道支持（`bind_back`） | ❌ | ✅ | ✅ |
| 支持 move-only 类型的 range 适配器 | ❌ | ✅ | ✅ |
| `ranges::generate_random` | ❌ | ❌ | ✅ |
| `views::concat` | ❌ | ❌ | ✅ |
| `views::cache_latest` / `views::as_input` | ❌ | ❌ | ✅ |
| `constexpr stable_sort` / `stable_partition` / `inplace_merge` | ❌ | ❌ | ✅ |

### 3.3 容器与数据结构

| 特性 | C++20 | C++23 | C++26 |
|------|:-----:|:-----:|:-----:|
| `std::span` | ✅ | ✅ | ✅ |
| `std::flat_map` / `std::flat_set` | ❌ | ✅ | ✅ |
| `std::mdspan`（多维 span） | ❌ | ✅ | ✅ |
| `std::inplace_vector<T, N>`（栈上定容 vector） | ❌ | ❌ | ✅ |
| `std::hive`（对象池集合，指针稳定） | ❌ | ❌ | ✅ |
| 关联容器异构擦除重载 | ❌ | ✅ | ✅ |
| 容器 Range 构造函数 | ❌ | ✅ | ✅ |
| `std::stack` / `std::queue` 迭代器对构造函数 | ❌ | ✅ | ✅ |
| `std::submdspan()` | ❌ | ❌ | ✅ |
| `std::lru_cache` | ❌ | ❌ | ✅（可能延期） |

### 3.4 智能指针与内存管理

| 特性 | C++20 | C++23 | C++26 |
|------|:-----:|:-----:|:-----:|
| `std::make_shared` 支持数组 | ✅ | ✅ | ✅ |
| `std::out_ptr` / `std::inout_ptr`（C 互操作） | ❌ | ✅ | ✅ |
| `constexpr std::unique_ptr` | ❌ | ✅ | ✅ |
| `std::start_lifetime_as` / `std::start_lifetime_as_array` | ❌ | ✅ | ✅ |
| `std::hazard_pointer`（无锁安全回收） | ❌ | ❌ | ✅ |
| `std::rcu`（Read-Copy-Update） | ❌ | ❌ | ✅ |

### 3.5 并发与异步

| 特性 | C++20 | C++23 | C++26 |
|------|:-----:|:-----:|:-----:|
| `std::jthread`（自动 join 线程） | ✅ | ✅ | ✅ |
| `std::latch` / `std::barrier` | ✅ | ✅ | ✅ |
| `std::semaphore` | ✅ | ✅ | ✅ |
| `std::atomic` 等待/通知 | ✅ | ✅ | ✅ |
| `std::atomic_ref` | ✅ | ✅ | ✅ |
| `std::move_only_function` | ❌ | ✅ | ✅ |
| `std::execution`（Sender/Receiver 框架） | ❌ | ❌ | ✅ |
| `std::copyable_function` | ❌ | ❌ | ✅ |

### 3.6 错误处理与调试

| 特性 | C++20 | C++23 | C++26 |
|------|:-----:|:-----:|:-----:|
| `std::source_location` | ✅ | ✅ | ✅ |
| `std::stacktrace` | ❌ | ✅ | ✅ |
| `std::expected<T, E>`（类似 Rust Result） | ❌ | ✅ | ✅ |
| `std::expected` monadic 操作（`and_then`、`transform` 等） | ❌ | ✅ | ✅ |
| `std::optional` monadic 操作 | ❌ | ✅ | ✅ |
| `std::unreachable()` | ❌ | ✅ | ✅ |
| `std::debugging`（`breakpoint`、`is_debugger_present`） | ❌ | ❌ | ✅ |
| 合约：`pre` / `post` / `contract_assert` | ❌ | ❌ | ✅ |

### 3.7 字符串与文本

| 特性 | C++20 | C++23 | C++26 |
|------|:-----:|:-----:|:-----:|
| `char8_t`（UTF-8 字符类型） | ✅ | ✅ | ✅ |
| `starts_with` / `ends_with`（`string` / `string_view`） | ✅ | ✅ | ✅ |
| `contains()`（`string` / `string_view`） | ❌ | ✅ | ✅ |
| `resize_and_overwrite()` | ❌ | ✅ | ✅ |
| `std::basic_string::substr() &&` | ❌ | ✅ | ✅ |
| 禁止从 `nullptr` 构造 `string` / `string_view` | ❌ | ✅ | ✅ |
| 字符串 + 字符串视图直接拼接 | ❌ | ❌ | ✅ |
| `std::bitset` 从 `string_view` 构造 | ❌ | ❌ | ✅ |
| `std::text_encoding`（IANA 字符集） | ❌ | ❌ | ✅ |

### 3.8 数学与数值

| 特性 | C++20 | C++23 | C++26 |
|------|:-----:|:-----:|:-----:|
| 数学常数 `<numbers>` | ✅ | ✅ | ✅ |
| `std::bit_cast` | ✅ | ✅ | ✅ |
| `std::midpoint` / `std::lerp` | ✅ | ✅ | ✅ |
| `std::cmp_equal` / `std::cmp_less` 等安全比较 | ✅ | ✅ | ✅ |
| `constexpr <cmath>` 数学函数 | ❌ | ✅ | ✅ |
| `constexpr <complex>` | ❌ | ✅ | ✅ |
| `std::byteswap` | ❌ | ✅ | ✅ |
| `std::to_underlying` | ❌ | ✅ | ✅ |
| `<simd>` 数据并行类型 | ❌ | ❌ | ✅ |
| `<linalg>` 线性代数（BLAS 接口） | ❌ | ❌ | ✅ |
| 饱和算术（`std::add_sat`、`std::div_sat` 等） | ❌ | ❌ | ✅ |
| 2022 SI 前缀（`quecto`、`ronto`、`ronna`、`quetta`） | ❌ | ✅ | ✅（补充） |
| `std::chrono` 值类型支持哈希 | ❌ | ❌ | ✅ |

### 3.9 其他杂项

| 特性 | C++20 | C++23 | C++26 |
|------|:-----:|:-----:|:-----:|
| `std::bind_front` | ✅ | ✅ | ✅ |
| `std::forward_like` | ❌ | ✅ | ✅ |
| `std::invoke_r` | ❌ | ✅ | ✅ |
| `std::is_scoped_enum` | ❌ | ✅ | ✅ |
| `std::is_implicit_lifetime` | ❌ | ✅ | ✅ |
| `std::is_within_lifetime` | ❌ | ❌ | ✅ |
| `import std;` 标准库模块 | ❌ | ✅ | ✅ |
| 弃用 `std::aligned_storage` / `std::aligned_union` | ❌ | ✅ | ✅ |
| `(signed) size_t` 字面量后缀 `z` / `uz` | ❌ | ✅ | ✅ |

---

## 4. 各版本重磅特性详解

### 4.1 C++20 四大支柱

#### 🔷 Concepts（概念）

```cpp
// 定义概念：约束类型必须支持 < 比较
template<typename T>
concept Sortable = requires(T a, T b) {
    { a < b } -> std::convertible_to<bool>;
};

// 使用概念约束模板
template<Sortable T>
void my_sort(std::vector<T>& v) { /* ... */ }

// 缩写语法
void print_all(const std::ranges::range auto& container) {
    for (const auto& item : container) std::cout << item;
}
```

**价值**：编译器错误从 300 行模板展开变成 3 行精准提示。

#### 🔷 Ranges（范围）

```cpp
std::vector<int> v = {1, 2, 3, 4, 5, 6};
auto result = v | std::views::filter([](int x) { return x % 2 == 0; })
                | std::views::transform([](int x) { return x * x; })
                | std::ranges::to<std::vector>();
// result = {4, 16, 36}
```

**价值**：告别 `begin()`/`end()` 对，管道式组合，惰性求值。

#### 🔷 Modules（模块）

```cpp
// math.ixx
export module math;
export int add(int a, int b) { return a + b; }

// main.cpp
import math;
int main() { return add(1, 2); }
```

**价值**：告别 `#include` 的宏污染、重编译、ODR 违规问题。

#### 🔷 Coroutines（协程）

```cpp
std::generator<int> fibonacci() {
    int a = 0, b = 1;
    while (true) {
        co_yield a;
        auto next = a + b; a = b; b = next;
    }
}
```

**价值**：原生异步编程，无需回调地狱，无状态机样板代码。

---

### 4.2 C++23 关键补全

#### 🟢 `std::expected<T, E>` — 官方错误处理方案

```cpp
std::expected<int, std::string> divide(int a, int b) {
    if (b == 0) return std::unexpected("division by zero");
    return a / b;
}

auto result = divide(10, 2)
    .and_then([](int v) { return v * 2; })   // monadic 链式调用
    .or_else([](auto& err) { /* 错误处理 */ });
```

**价值**：无需异常开销的错误传播，借鉴 Rust 的 `Result`。

#### 🟢 `deducing this` — 显式对象参数

```cpp
struct Base {
    template<typename Self>
    void visit(this Self&& self) {   // C++23
        // Self 会推导为 Base&, const Base&, Base&& 等
    }
};
```

**价值**：终结 CRTP 模式，模板成员函数不再需要重复声明 4 个 `const`/`&`/`&&` 重载。

#### 🟢 `std::generator<T>` — 开箱即用的协程生成器

```cpp
std::generator<int> range(int from, int to) {
    for (int i = from; i < to; ++i) co_yield i;
}
```

**价值**：C++20 协程只需手动实现 `promise_type`，C++23 标准库直接提供。

#### 🟢 `std::mdspan` — 多维数组视图

```cpp
std::vector<double> data(100);
std::mdspan mat(data.data(), 10, 10);  // 10x10 矩阵视图
mat[3, 4] = 1.0;                        // 多维下标访问
```

**价值**：科学计算/数值密集型程序不再需要第三方多维数组库。

#### 🟢 `std::print` / `std::println` — 新时代 I/O

```cpp
std::println("Hello, {}! The answer is {}.", "world", 42);
// 比 std::cout 更简洁，比 printf 更安全
```

#### 🟢 `std::stacktrace` — 运行时调用栈

```cpp
void bar() {
    auto trace = std::stacktrace::current();
    std::cout << trace << '\n';  // 输出完整调用栈
}
```

---

### 4.3 C++26 四大突破

#### 🔴 编译期反射（Reflection）

```cpp
#include <meta>

struct Point { int x; int y; };

// 自动生成 operator<<
template<typename T>
void print_members(const T& obj) {
    // 使用 constexpr 反射遍历所有成员
    constexpr auto members = std::meta::members_of(^^T);
    [:expand(members):] >> {
        std::println("  {} = {}", std::meta::name_of(member), obj.[:member:]);
    };
}
```

**价值**：**零运行时开销**的序列化、ORM、RPC 代码自动生成。被称为"C++ 有史以来最强大的抽象引擎"。

#### 🔴 合约（Contracts）

```cpp
#include <contracts>

int divide(int a, int b)
    pre(b != 0)                          // 前置条件
    post(result * b == a)                // 后置条件
{
    contract_assert(a >= 0);             // 断言
    return a / b;
}
```

四种违规处理模式：
| 模式 | 行为 |
|------|------|
| `ignore` | 不检查 |
| `observe` | 日志记录，继续执行 |
| `enforce` | 调用 `std::abort()` |
| `quick_enforce` | 最小检查后 `abort()` |

**价值**：设计即契约（Design by Contract），取代有 50 年历史的 C `assert` 宏。

#### 🔴 内存安全（Memory Safety）

```
关键改变：
1. 未初始化局部变量读取 → 不再触发 UB，变为"Erroneous Behavior"（错误行为）
2. 标准库边界加固 → vector、span、string 等默认启用边界检查
```

**实际效果（Google 数据）**：
- 修复了 **1000+** 个 bug
- segfault 率降低 **30%**
- 性能开销仅约 **0.3%**

#### 🔴 `std::execution`（结构化异步）

```cpp
#include <execution>

// Sender/Receiver 模型
auto work = stdexec::schedule(scheduler)
          | stdexec::then([] { return compute_value(); })
          | stdexec::then([](int v) { return v * 2; });
auto [result] = stdexec::sync_wait(work).value();
```

**核心组件**：
- **Senders**：描述异步工作（惰性，不立即执行）
- **Receivers**：消耗异步结果
- **Schedulers**：控制执行上下文（线程池、GPU 等）

**价值**：**结构化并发** — 数据竞争在设计层面就被消除。统一了线程池、协程、SIMD 的执行模型。

#### 🔴 其他 C++26 亮点

```cpp
// #embed — 编译期嵌入二进制资源
constexpr std::span<const std::byte> icon = {
    #embed "icon.png"
};

// <simd> — 数据并行
std::simd<float> a = {1.0f, 2.0f, 3.0f, 4.0f};
std::simd<float> b = a * 2.0f + 1.0f;  // 自动使用 AVX/NEON

// <linalg> — 线性代数
std::mdspan mat(data, M, N);
std::vector<double> vec(N);
std::linalg::matrix_vector_product(mat, vec, result);

// 饱和算术
int x = std::add_sat(INT_MAX, 1); // 返回 INT_MAX，不溢出

// = delete("message")
void dangerous_api() = delete("Use safe_api() instead");

// 包索引
template<typename... Ts>
auto first(Ts&&... args) {
    return std::get<0>(std::tuple(args...));
    // C++26: return args...[0];
}
```

---

## 5. 编译器支持现状

| 特性集 | GCC | Clang | MSVC |
|--------|-----|-------|------|
| **C++20 核心** | ✅ 11+ 完整 | ✅ 16+ 完整 | ✅ VS 2022 17.0+ 完整 |
| **C++20 std::format** | ✅ 13 | ✅ 14+ | ✅ VS 2022 17.2+ |
| **C++23 核心语言** | ✅ 14+ 基本完整 | ✅ 18+ 基本完整 | ✅ VS 2022 17.8+ 基本完整 |
| **C++23 std::expected** | ✅ 12 | ✅ 16 | ✅ VS 2022 17.3 |
| **C++23 std::generator** | ✅ 14 | ✅ 19 | ⚠️ 部分 |
| **C++23 std::stacktrace** | ✅ 14 | ⚠️ libc++ 未完成 | ✅ VS 2022 17.5 |
| **C++26 Reflection** | ✅ GCC 16（主干） | ⚠️ 进行中 | ⚠️ 进行中 |
| **C++26 Contracts** | ✅ GCC 16（主干） | ⚠️ 进行中 | ⚠️ 进行中 |
| **C++26 std::execution** | ⚠️ 进行中 | ⚠️ 进行中 | ⚠️ 进行中 |
| **C++26 #embed** | ✅ 15 | ✅ 19 | ⚠️ 进行中 |
| **C++26 <simd>** | ✅ GCC 13+ | ❌ | ⚠️ 进行中 |

---

## 6. 迁移建议

### 立即行动（2026 年）

| 如果当前是 | 推荐目标 | 理由 |
|-----------|---------|------|
| **C++17** | **C++20** 或 **C++23** | C++20 三大编译器已完整支持，风险极低 |
| **C++20** | **C++23** | 零痛点迁移，主要是标准库 API 增强 |
| **C++23** | 关注 **C++26** | 等待编译器成熟即可获得四大革命性特性 |

### 新项目推荐

```
优先级：C++23 > C++20 > C++17

C++23 是 C++20 的"生产就绪版"：
  - std::expected 提供了惯用的错误处理
  - std::print 提供了更好的 I/O
  - std::generator 让协程开箱即用
  - 编译器支持已相当成熟
  - import std; 已可用

2026 年底后可考虑切换至 C++26（等编译器支撑到位）
```

### 渐进式采用 C++26 特性

```cpp
// 可以先行使用这些编译器已支持的 C++26 特性：
- #embed（GCC 15+ / Clang 19+）
- = delete("reason")（Clang 19+ / MSVC VS 2022 17.12+ / GCC 16+）
- 占位符变量 _（Clang 19+）
- static operator[]（GCC 14+）
- 饱和算术（GCC 14+）
- <simd>（GCC 13+）
```

### 核心建议

1. **C++17 用户**：直接跳到 C++23，享受两代红利
2. **C++20 用户**：无缝升级 C++23，尤其建议采用 `std::expected` 替代原始错误码
3. **所有用户**：即使不立即升级编译器，也应学习 C++26 的反射和 `std::execution` 理念——它们代表了 C++ 未来的编程范式

---

> **数据来源与参考**
> - [C++26: Reflection, Memory Safety, Contracts, and a New Async Model (InfoQ)](https://www.infoq.com/news/2026/04/cpp-26-reflection-safety-async/)
> - [Trip Report: November 2025 ISO C++ Standards Meeting (Herb Sutter)](https://herbsutter.com/2025/11/10/trip-report-november-2025-iso-c-standards-meeting-kona-usa/)
> - [C++26 维基百科](https://zh.wikipedia.org/wiki/C%2B%2B26)
> - [C++ Standards Support in GCC](https://gcc.gnu.org/projects/cxx-status.html)
> - [Contracts are in C++26 (The Register)](https://www.theregister.com/2026/03/31/cplusplus26_approved/)
> - [cppreference.com — C++ Compiler Support](https://en.cppreference.com/w/cpp/compiler_support)
