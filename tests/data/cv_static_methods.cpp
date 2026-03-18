struct StaticMethodClass
{
    static int GetConstant()
    {
        return 100;
    }
};

StaticMethodClass global_static_inst;

int call_static()
{
    return StaticMethodClass::GetConstant();
}
