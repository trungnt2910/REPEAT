namespace
{
struct AnonStruct
{
    int m_val;
};
} // namespace

int call_anon()
{
    AnonStruct local_var = {42};
    return local_var.m_val;
}
