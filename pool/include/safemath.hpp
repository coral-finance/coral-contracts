namespace safemath {
    using std::string;
    uint64_t add(const uint64_t a, const uint64_t b) {
        uint64_t c = a + b;
        check(c >= a, "add-overflow"); return c;
    }

    uint64_t sub(const uint64_t a, const uint64_t b) {
        uint64_t c = a - b;
        check(c <= a, "sub-overflow"); return c;
    }

    uint64_t mul(const uint64_t a, const uint64_t b) {
        uint64_t c = a * b;
        check(b == 0 || c / b == a, "mul-overflow"); return c;
    }

    uint64_t div(const uint64_t a, const uint64_t b) {
        check(b > 0, "divide by zero");
        return a / b;
    }
} // namespace safemath