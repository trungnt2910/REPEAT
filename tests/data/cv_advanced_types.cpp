#define FORCE_INLINE __attribute__((always_inline))

#define JUMP_200()                                                                                 \
    asm volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; "                              \
                 "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; "                              \
                 "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; "                              \
                 "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; "                              \
                 "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; "                              \
                 "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; "                              \
                 "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; "                              \
                 "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; "                              \
                 "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; "                              \
                 "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; "                              \
                 "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; "                              \
                 "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; "                              \
                 "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; "                              \
                 "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; "                              \
                 "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; "                              \
                 "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; "                              \
                 "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; "                              \
                 "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; "                              \
                 "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; "                              \
                 "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;")

typedef int MyInteger;

// Forward declarations of incomplete class/union types. These are used to verify
// that the translator correctly handles pointers to incomplete types without
// attempting to resolve full debug info.
class ForwardClass;
union ForwardUnion;

// Extern variable declaration with no location info to trigger std::nullopt paths
extern int extern_variable_no_loc;

class AdvancedClass
{
private:
    MyInteger m_privateField;

protected:
    const int m_protectedField;

public:
    volatile bool m_publicBool;
    long long m_publicInt64;
    short m_publicInt16;

    // Unsigned types to cover more basic type size paths
    unsigned char m_uChar;
    unsigned short m_uShort;
    unsigned long long m_uLongLong;

    // Pointers to incomplete types to verify that CodeView generation handles
    // forward declarations without full type info (ensuring it emits LF_POINTER
    // to LF_CLASS/LF_UNION with the forward reference flag set).
    ForwardClass* m_forwardClassPtr;
    ForwardUnion* m_forwardUnionPtr;

    // Nested class definition to trigger NestedTypeRecord
    class NestedInner
    {
        int m_val;
    };

    AdvancedClass()
        : m_privateField(10),
          m_protectedField(20),
          m_publicBool(true),
          m_publicInt64(0),
          m_publicInt16(0),
          m_uChar(0),
          m_uShort(0),
          m_uLongLong(0),
          m_forwardClassPtr(nullptr),
          m_forwardUnionPtr(nullptr)
    {
    }

private:
    FORCE_INLINE void PrivateMethod()
    {
        m_privateField++;
    }

protected:
    FORCE_INLINE void ProtectedMethod()
    {
        int x = m_protectedField;
        (void)x;
    }
};

// Explicit accessibility in a struct to force DW_AT_accessibility attributes
struct AdvancedStruct
{
private:
    int m_structPrivate;

protected:
    int m_structProtected;

public:
    int m_structPublic;
};

// Global inlined helper with huge code delta to trigger line compression > 128
static inline __attribute__((always_inline)) int my_inlined_helper(int x)
{
    JUMP_200(); // Forces a large instruction offset for line number mapping.
    return x + 5;
}

// Function with 6 parameters to trigger DWARF register location mapping:
// p_rdi (reg5), p_rsi (reg4), p_rdx (reg1), p_rcx (reg2), p_r8 (reg8), p_r9 (reg9)
static void test_parameter_registers(int p_rdi, int p_rsi, int p_rdx, int p_rcx, int p_r8, int p_r9)
{
    int volatile sum = p_rdi + p_rsi + p_rdx + p_rcx + p_r8 + p_r9;
    (void)sum;
}

int main()
{
    AdvancedClass obj;
    obj.m_publicBool = false;
    obj.m_publicInt64 = 1234567890LL;
    obj.m_publicInt16 = 32000;
    obj.m_uChar = 'A';
    obj.m_uShort = 50000;
    obj.m_uLongLong = 9999999999999ULL;

    // Call an inlined function with a large code block to verify that the line
    // translator correctly handles line number delta compression (offsets > 128).
    int val = my_inlined_helper(10);
    (void)val;

    // Call a function with 6 parameters to verify that DWARF register location
    // mapping correctly translates DWARF registers to CodeView registers for parameters.
    test_parameter_registers(10, 20, 30, 40, 50, 60);

    // Verify handling of variables with no defined location.
    int volatile use_extern = extern_variable_no_loc;
    (void)use_extern;

    AdvancedStruct str;
    (void)str;

    // Register variables to trigger map_dwarf_reg_to_cv_reg
    register int r_rax asm("rax") = 1;
    register int r_rbx asm("rbx") = 2;
    register int r_rcx asm("rcx") = 3;
    register int r_rdx asm("rdx") = 4;
    register int r_rsi asm("rsi") = 5;
    register int r_r8 asm("r8") = 8;

    // Force a line change after a large block of code
    JUMP_200();
    obj.m_publicInt16 = r_rax + r_rbx + r_rcx + r_rdx + r_rsi + r_r8;

    return 0;
}
