unsigned long long __udivdi3(unsigned long long a, unsigned long long b) {
    unsigned long long res = 0;
    if(b == 0) return 0;
    while(a >= b) { a -= b; res++; }
    return res;
}
unsigned long long __umoddi3(unsigned long long a, unsigned long long b) {
    if(b == 0) return 0;
    while(a >= b) a -= b;
    return a;
}
