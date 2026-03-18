struct Base
{
    int m_b;
};

struct Derived : public Base
{
    int m_d;
};

Derived global_derived = {{1}, 2};
