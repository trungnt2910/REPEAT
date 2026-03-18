namespace ns
{
struct Outer
{
    struct Inner
    {
        int m_x;
    };
    Inner m_innerVar;
};
} // namespace ns

ns::Outer global_outer = {{42}};
