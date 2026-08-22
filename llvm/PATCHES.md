# 这棵 vendored 树被动过的地方

⭐ **规矩三条,与 `openkal-musl` 的一致:**

1. **可以动源码。** 移植就是动源码;假装不动只会把差异藏进别处。
2. **动过的地方要标注清楚。** 每一处都夹在
   `// ─── openkal ─── BEGIN` / `// ─── openkal ─── END` 之间,`grep` 一次能数完。
3. **能不动的就不动。** 上游能自己走对的路,让它走 —— 见下面的「不需要动的」。

```sh
# 数一遍
grep -rn "openkal ─── BEGIN" llvm/
```

---

## 判据:换的是「平台」,不是「库」

上游按**操作系统**选实现;openkal 的答案按**下面配置的是什么**。所以每一处替换
都必须能填进这句话:

> upstream 在这里问「这是哪个 OS」,而这个问题真正的答案是 openkal 的 `<接口>`。

⚠️ 填不进去的**不要动**。`sizeof(long)`、`int64_t` 的拼法、目标格式、调用约定
不是「下面是谁」,是**目标自己的定义** —— 那些归 C 库(`musl-generated/` 按
(arch, os) 分档),不归这里。

---

## 已替换

⭐ 五处,四个文件,一个判据 —— 每一处都能填进上面那句话。

```sh
grep -rn "openkal ─── BEGIN" llvm/    # 10 处标记(含配对的 #else/#endif)
```

| 文件 | 上游按什么分派 | 平台面 | openkal 的答案 |
|---|---|---|---|
| `libcxx/src/atomic.cpp` | 四个 OS | 挂起/唤醒 | `kal_task_wait` / `kal_task_wake` |
| `libunwind/src/AddressSpace.hpp` | `_WIN32` | **这个镜像的展开表在哪** | 镜像自己知道:`__ImageBase` + 段表 |
| `libunwind/src/RWMutex.hpp` | `_WIN32` | 读写锁 | pthread ⇒ musl ⇒ `kal_task_*` |
| `libunwind/src/UnwindCursor.hpp` | `_WIN32` | (无) | 整段没被用到,只是被 include |
| `compiler-rt/…/emutls.c` | `_WIN32` | 互斥 + 对齐分配 | pthread + `posix_memalign` |

### `libcxx/src/atomic.cpp` — `std::atomic` 的等待与唤醒

上游一条链,五个分支,四个操作系统:

```
__linux__   → syscall(SYS_futex, …)
__FreeBSD__ → _umtx_op
__OpenBSD__ → futex()
_WIN32      → WaitOnAddress / WakeByAddressSingle
否则         → 完全不等待(自旋)
```

平台面**恰好两个函数**(`__platform_wait_on_address` / `__platform_wake_by_address`),
其余 495 行全是可移植的 —— 所以这里是**原地打标记的补丁**而不是整文件拷贝,
漂移面从 495 行缩到约 50 行。

openkal 的答案是 `kal_task_wait` / `kal_task_wake` —— 规范管它叫**挂起原语**,
而它在每一个目标上是同样的两个调用,**包括底下没有操作系统的那个**。

⚠️ **它本来就已经到 openkal 了,只是绕了一圈。** 在 Linux 上,上游那条分支发
`SYS_futex`,openkal-musl 的 port 拦下这个系统调用,它的 `__okm_futex` 调
`kal_task_wait`。直接走去掉了那一圈,并且让另外两种目标格式也能用 —— 在那里
「拦系统调用」没有系统调用可拦。

⚠️ 两处细节按上游的语义保留:两秒的默认超时(没有超时的等待注意不到与它竞争的
唤醒),以及 `-1` 表示「全部唤醒」(上游写作 `INT_MAX`,openkal 的计数是
`kal_uintptr`,所以按回绕写)。

### ⭐⭐ `libunwind/src/AddressSpace.hpp` — 这个镜像的展开表在哪

上游按 OS 答:`EnumProcessModules` 枚举进程的全部模块,逐个解析 PE 头找
`.eh_frame`。**而这个问题这份文件已经答过三遍,没有一遍问操作系统**:

| 目标 | 怎么答的 |
|---|---|
| 裸机 | 链接器脚本定义的 `__eh_frame_start` / `__eh_frame_end` |
| Darwin | `_dyld_find_unwind_sections`(`openkal-macos` 用链接器的 `section$start$` 实现) |
| ELF | 自己的 program headers |
| **PE(上游)** | ⚠️ 让操作系统枚举模块 |

⇒ openkal 的答案和前三条同一句话:**镜像自己知道**。静态链接的 openkal 程序
只有一个模块,基址是链接器定义的符号 `__ImageBase`(不是调用),段表在离它固定
的偏移上。读它是**目标格式**的知识 —— 展开器本来就是由这种知识构成的 —— 不是
系统调用,也不是操作系统。

⚠️ **而台账原先写着这一支「已由 DWARF 路线绕开」,那是凭读守卫写的,实测否掉了。**
`_WIN32 && DWARF` 正是它的守卫,DWARF 路线就是它。2026-08-23 实测:

```
AddressSpace.hpp:114 → /usr/x86_64-w64-mingw32/include/windows.h:69
  → winnt.h:1658 → x86intrin.h → mm_malloc.h:43
    error: use of undeclared identifier '__mingw_aligned_malloc'
```

—— 一条 include 把**宿主的 mingw sysroot** 拉进了刚刚摆脱了厂商 SDK 的构建。

⭐ 实测确认段表里找得到:链接后的镜像里是 `.eh_fram`(PE 的段名字段固定 8 字节,
`.eh_frame` 是 9 个字符),大小 0x530c8 —— 这正是上游用
`IMAGE_SIZEOF_SHORT_NAME` 比 8 个字节而不是比全名的原因。

⚠️ **走过一条错路,记下来免得再走**:先试过用 COFF 的分组段
(`.eh_frame$a` / `.eh_frame$z`)去夹住 `.eh_frame`,链接器的排序是

```
.eh_frame    0x00  0x58   ← 真正的帧
.eh_frame$a  0x58  0x01   ← "start" 落在它们后面
.eh_frame$z  0x59  0x01   ⇒ end-start = 1 字节
```

**不带 `$` 的段排在最前**,所以那个方案会给展开器一个空区间 —— 不是链接失败,
是**静默的错答案**。

### `libunwind/src/RWMutex.hpp` — 读写锁

⚠️ 两处,而不是一处:`<windows.h>` 的 include 在 `_LIBUNWIND_HAS_NO_THREADS`
**之前**就无条件发生,类的选择是第二处。只撤回第一处会留下引用 `SRWLOCK` 而没有
任何东西声明它。

### `libunwind/src/UnwindCursor.hpp` — 一处纯粹没被用到的 include

这份文件里所有需要 Windows 类型的代码都在 `_LIBUNWIND_SUPPORT_SEH_UNWIND` 下,
而 openkal 用 `-fdwarf-exceptions`。⇒ include 被执行,它声明的东西一个都没用上,
而它拉进来的是宿主的 mingw sysroot。

### `compiler-rt/lib/builtins/emutls.c` — 互斥与对齐分配

⚠️ **这个文件被编译到 PE 上,恰恰是因为那个平台自己的 thread-local 机制用不了**
(`_tls_index` 需要动态加载器),然后它转身去要了同一个平台的 C 运行时:

```
corecrt.h:98: typedef redefinition with different types
emutls.c:164: call to undeclared function '_aligned_malloc'
```

---

## ⭐ 一个名字,不是五个

五处补丁全部守卫在 **`OPENKAL`** 上,`cflags` 和 `cxxflags` 各给一次
(`compiler-rt` 是 C)。读法是「在 Windows 上,除非底下是 openkal」。

⚠️ 一度写成 `_LIBUNWIND_OPENKAL`,随后 `emutls.c` 需要同一个事实而它是 C 文件 ——
**同一个事实两个名字**正是这套代码里反复出问题的形状,所以收敛掉了。

---

## 不需要动的 —— 而这一节比上一节重要

⭐ 整棵树里 `#include <windows.h>` 共 **19 处**。按守卫分类:

| 守卫 | 处数 | 怎么解决的 |
|---|---|---|
| `_LIBCPP_WIN32API` | **12** | ✅ `port/include/__config` 撤回了它 → **自己落到 POSIX 分支** |
| `__SEH__` / `_LIBUNWIND_SUPPORT_DWARF_UNWIND` | 2 | ✅ `-fdwarf-exceptions`,异常机制跟随我们带的 unwinder |
| 裸 `_WIN32` | 3 | ✅ 三处全换(AddressSpace / RWMutex / UnwindCursor) |
| `emutls.c` 的 `_WIN32` | 2 | ✅ 走 POSIX 分支 |

⭐⭐ **12 处不用换,因为 libc++ 的 POSIX 分支本来就已经在 openkal 上了。** 它调
`fopen` / `clock_gettime` / `pthread_*`,那些是 musl,而 musl 就在 openkal 上。
让谓词答对,它自己就走到那条路。

⇒ 这就是为什么整份移植是几十行而不是把 libc++ 重做一遍。**先问「上游有没有一条
路已经通向 openkal」,再考虑换。**

---

## ⚠️ 不在这里的两类东西

一开始误以为要在这棵树里解决,实际不在:

| | 归谁 | 为什么 |
|---|---|---|
| 数据模型(`sizeof(long)`) | **C 库**(`musl-generated/<arch>[-<os>]`) | 它重建的是 POSIX,而 POSIX 自己命名了 `long`。openkal 接口上不存在这个问题 —— 见 openkal 的 §1.3 |
| `-fdwarf-exceptions` / `-femulated-tls` | **mcpp**(`graph_runtime_compile_flags`) | 它们决定 `throw` 和 `thread_local` **编译成什么**,所以是**整张图**的性质,不是某个包的。写在本包 `[build]` 里时,只覆盖了本包的对象,使用者的 `main.o` 拿不到 |

⚠️ 第二行是实测逼出来的:全部编过之后,链接报
`undefined symbol: __gxx_personality_seh0`,引用它的是**使用者的 `main.o`** ——
一个写了 `try` 而没有任何理由知道这件事的翻译单元。

---

## 与 `port/include/` 的分工

| | 放哪 | 为什么 |
|---|---|---|
| **头文件**里的平台分派 | `port/include/` 覆盖 | 靠 include 顺序遮蔽,vendored 树逐字节不动,**不随上游漂移** |
| **源码**里的平台分派 | 本文件记录的原地标记 | 消费者不 include `.cpp`,遮蔽不了 |

⚠️ 顺序是有偏好的:**能用覆盖就别用补丁**。覆盖的漂移面是零,补丁的漂移面是被
标记的那几十行。
