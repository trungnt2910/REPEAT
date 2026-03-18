struct MethodClass
{
    int m_val;
    int GetVal()
    {
        return m_val;
    }
};

MethodClass global_method_class = {42};
