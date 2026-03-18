struct Simple
{
    int a;
};

struct Nested
{
    struct Simple s;
    double d;
};

struct Nested global_nested = {{10}, 2.5};
