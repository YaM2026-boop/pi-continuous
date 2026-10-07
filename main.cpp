// ============================================================
//  圆周率不停计算器  (C++ / MinGW-w64 / VS Code)
//  ------------------------------------------------------------
//  算法一(主): Chudnovsky 级数 + 二分分裂 (Binary Splitting)
//             每项约产生 14 位十进制数字, 是大数 π 计算的主流算法
//  算法二(验): Machin 公式  π = 16·atan(1/5) − 4·atan(1/239)
//             与算法一完全不同的数学路线, 用于交叉验证
//
//  构建: g++ -O2 -std=c++17 -static -o main.exe main.cpp
// ============================================================

#include <boost/multiprecision/cpp_int.hpp>
#include <windows.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using boost::multiprecision::cpp_int;
using std::string;

// ============================================================
//  算法二: Machin 公式 (独立验证用)
// ============================================================
static cpp_int arctan_inv(long x, long scaleDigits) {
    // 计算 atan(1/x) * 10^scaleDigits
    cpp_int one(1);
    for (long i = 0; i < scaleDigits; ++i) one *= 10;

    cpp_int xpow = one / x;   // (1/x) 的定点表示
    const cpp_int xsq = cpp_int(x) * x;

    cpp_int sum = 0, term = xpow;
    for (long n = 0; term != 0; ++n) {
        cpp_int add = term / (2 * n + 1);
        if (n % 2 == 0) sum += add; else sum -= add;
        if (term < xsq) break;
        term /= xsq;
    }
    return sum;
}

static string pi_machin(int digits) {
    // 多留 50 位缓冲: 定点截断会累积误差, 缓冲不足会污染尾部数字
    const long scale = digits + 50;
    cpp_int pi = 16 * arctan_inv(5, scale) - 4 * arctan_inv(239, scale);
    string s = pi.convert_to<string>();
    if ((int)s.size() < digits + 1) s = string(digits + 1 - s.size(), '0') + s;
    s = s.substr(0, digits + 1);   // "3141592..."
    s.insert(1, ".");              // 与 pi_chudnovsky 统一为 "3.141592..."
    return s;
}

// ============================================================
//  算法一: Chudnovsky + 二分分裂
//    1/π = 12 · Σ (−1)^k (6k)! (13591409 + 545140134k)
//                 / ((3k)! (k!)^3 · 640320^(3k+3/2))
// ============================================================
struct BS { cpp_int P, Q, T; };

// 区间 [a, b) 的二分分裂
static BS bs(int a, int b) {
    if (b - a == 1) {
        if (a == 0) return BS{ cpp_int(1), cpp_int(1), cpp_int(13591409) };
        // 递推式 (a>=1): 分子分母用当前 k 直接构造, 避免符号错误
        const long k = a;
        // (6k-5)(2k-1)(6k-1)
        cpp_int num = cpp_int(6 * k - 5) * (2 * k - 1) * (6 * k - 1);
        // k^3 · 640320^3 / 24
        cpp_int den = cpp_int(k) * k * k;
        den *= cpp_int(640320); den *= 640320; den *= 640320;
        den /= 24;
        cpp_int t = cpp_int(13591409) + cpp_int(545140134) * k;
        if (k % 2 == 1) t = -t;
        return BS{ num, den, t * num };
    }
    const int m = (a + b) / 2;
    const BS L = bs(a, m), R = bs(m, b);
    return BS{ L.P * R.P, L.Q * R.Q, L.T * R.Q + L.P * R.T };
}

// 整数牛顿法开平方: 返回 floor(sqrt(n))
// 注意: 不能用 boost::multiprecision::sqrt —— 它走浮点子程序, 位宽不足时
//       会静默截断(实测只给到约 1/3 的位数), 导致结果整体失真。
static cpp_int isqrt(cpp_int n) {
    if (n <= 0) return 0;
    if (n < 2) return n;
    const int len = (int)n.convert_to<string>().size();
    cpp_int x = cpp_int(10);
    for (int i = 1; i < (len + 1) / 2; ++i) x *= 10;   // 初值 ≈ 10^((len+1)/2)
    for (;;) {
        cpp_int y = (x + n / x) / 2;
        if (y >= x) break;
        x = y;
    }
    return x;
}

// 返回含整数位的 digits+1 位字符串, 如 "3.14159..."
static string pi_chudnovsky(int digits) {
    const int terms = (int)(digits / 14.181647462) + 2;
    const BS r = bs(0, terms);

    // 备用精度: 多留 50 位缓冲
    const int extra = digits + 50;
    cpp_int one = cpp_int(1);
    for (int i = 0; i < extra; ++i) one *= 10;

    // sqrt(10005) 定点化: isqrt(10005 * 10^(2*extra)) ≈ sqrt(10005) * 10^extra
    const cpp_int sq = isqrt(cpp_int(10005) * one * one);
    const cpp_int pi = (cpp_int(426880) * sq * r.Q) / r.T;

    string s = pi.convert_to<string>();
    if ((int)s.size() < digits + 1) s = string(digits + 1 - s.size(), '0') + s;
    s = s.substr(0, digits + 1);               // "3141592..."
    s.insert(1, ".");                          // "3.141592..."
    return s;
}

// ============================================================
//  工具
// ============================================================
static string with_commas(long long v) {
    string s = std::to_string(v), out;
    int c = 0;
    for (int i = (int)s.size() - 1; i >= 0; --i) {
        out.push_back(s[i]);
        if (++c % 3 == 0 && i != 0) out.push_back(',');
    }
    std::reverse(out.begin(), out.end());
    return out;
}

static bool enter_pressed() {
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    if (h == INVALID_HANDLE_VALUE || h == nullptr) return false;
    DWORD n = 0;
    if (!GetNumberOfConsoleInputEvents(h, &n) || n == 0) return false;
    INPUT_RECORD rec[32];
    DWORD got = 0, hit = 0;
    while (PeekConsoleInputA(h, rec, 32, &got) && got > 0) {
        for (DWORD i = 0; i < got; ++i) {
            if (rec[i].EventType == KEY_EVENT && rec[i].Event.KeyEvent.bKeyDown) {
                WORD vk = rec[i].Event.KeyEvent.wVirtualKeyCode;
                if (vk == VK_RETURN) hit = 1;
            }
        }
        if (!ReadConsoleInputA(h, rec, 32, &got)) break;
    }
    return hit != 0;
}

// ============================================================
int main(int argc, char** argv) {
    SetConsoleOutputCP(65001);

    long step  = 10000;      // 每批新增位数
    long stop  = 0;          // >0 时算够这么多位就退出
    bool quiet = false;

    for (int i = 1; i < argc; ++i) {
        const string a = argv[i];
        if (a == "--step"  && i + 1 < argc) step  = std::atol(argv[++i]);
        else if (a == "--stop"  && i + 1 < argc) stop  = std::atol(argv[++i]);
        else if (a == "--quiet") quiet = true;
        else if (a == "--help") {
            std::cout << "用法: main [--step N] [--stop M] [--quiet]\n"
                         "  --step N  每次新增 N 位数字并输出一次\n"
                         "  --stop M  算到 M 位后退出(默认一直算)\n"
                         "  --quiet   只输出数字(便于重定向)\n";
            return 0;
        }
    }

    if (step < 100)   step = 100;
    if (step > 200000) step = 200000;

    const auto t0 = std::chrono::steady_clock::now();

    if (!quiet) {
        std::cout << "==================================================\n"
                  << "  圆周率不停计算器\n"
                  << "  算法: Chudnovsky + 二分分裂 (自检: Machin 公式)\n"
                  << "  每批: " << with_commas(step) << " 位\n"
                  << "--------------------------------------------------\n";
    }

    long digits = 0;          // 当前已产生的位数(不含整数位)
    long verifyDigits = 0;    // 交叉验证使用的固定位数
    string machinRef;         // Machin 参考值(只算一次)

    for (;;) {
        digits += step;

        const auto tb = std::chrono::steady_clock::now();
        string s = pi_chudnovsky((int)digits);
        const auto te = std::chrono::steady_clock::now();
        const double batchSec =
            std::chrono::duration<double>(te - tb).count();

        // 交叉验证: 用固定的 verifyDigits 位作为稳定基准, 只算一次
        if (!quiet) {
            if (machinRef.empty()) {
                verifyDigits = std::min<long>(digits, 2000);
                machinRef = pi_machin((int)verifyDigits);
            }
            const string mainPart = s.substr(0, verifyDigits + 1);
            // 逐位比较: 不要直接用 ==, 否则长度不同会直接判不相等
            const size_t n = std::min(mainPart.size(), machinRef.size());
            if (mainPart.substr(0, n) != machinRef.substr(0, n)) {
                std::cerr << "\n[自检失败] Chudnovsky 与 Machin 不一致!\n"
                          << "  Chudnovsky: " << mainPart    << "\n"
                          << "  Machin    : " << machinRef   << "\n";
                return 2;
            }
        }

        if (quiet) {
            std::cout << s << std::endl;
        } else {
            const double totalSec =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            std::cout << "\n[第 " << (digits / step) << " 批] 已算 "
                      << with_commas(digits) << " 位"
                      << "   本批耗时 " << batchSec << " 秒"
                      << "   累计 " << totalSec << " 秒\n"
                      << "  自检: Chudnovsky 与 Machin 前 "
                      << with_commas(verifyDigits) << " 位一致 ✓\n";
            std::cout << (digits <= 100 ? s : s.substr(0, 102) + " ... " +
                          s.substr(s.size() - 32)) << std::endl;
            std::cout.flush();
        }

        if (stop > 0 && digits >= stop) break;

        if (!quiet && enter_pressed()) {
            std::cout << "\n[收到回车] 当前共 " << with_commas(digits)
                      << " 位, 已停止。\n";
            break;
        }
    }

    std::cout.flush();
    return 0;
}
