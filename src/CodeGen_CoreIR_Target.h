#ifndef HALIDE_CODEGEN_COREIR_TARGET_H
#define HALIDE_CODEGEN_COREIR_TARGET_H

/** \file
 *
 * Defines an IRPrinter that emits HLS C++ code.
 */

#include "CodeGen_CoreIR_Base.h"
#include "Module.h"
#include "Scope.h"

#include "coreir.h"

namespace Halide {

namespace Internal {

struct CoreIR_Argument {
    std::string name;

    bool is_stencil;

    Type scalar_type;

    CodeGen_CoreIR_Base::Stencil_Type stencil_type;
};

/** This class emits Xilinx Vivado HLS compatible C++ code.
 */
class CodeGen_CoreIR_Target {
public:
    /** Initialize a C code generator pointing at a particular output
     * stream (e.g. a file, or std::cout) */
    CodeGen_CoreIR_Target(const std::string &name);
    virtual ~CodeGen_CoreIR_Target();

    void init_module();

    void add_kernel(Stmt stmt,
                    const std::string &name,
                    const std::vector<CoreIR_Argument> &args);

    void dump();

protected:
    class CodeGen_CoreIR_C : public CodeGen_CoreIR_Base {
    public:
  CodeGen_CoreIR_C(std::ostream &s, OutputKind output_kind);
  ~CodeGen_CoreIR_C();

        void add_kernel(Stmt stmt,
                        const std::string &name,
                        const std::vector<CoreIR_Argument> &args);
    protected:
        std::string print_stencil_pragma(const std::string &name);

        using CodeGen_CoreIR_Base::visit;

        void visit(const For *op);
        void visit(const Allocate *op);
	void visit(const Call *op);
	void visit(const Provide *op);
        void visit(const Load *op);
        void visit(const Store *op);

        // coreir operators
	void visit_binop(Type t, Expr a, Expr b, const char* op_sym, string op_name);
	void visit(const Mul *op);
	void visit(const Add *op);
	void visit(const Sub *op);
	void visit(const EQ *op);
	void visit(const LT *op);
	void visit(const LE *op);
	void visit(const GT *op);
	void visit(const GE *op);
	void visit(const Cast *op);

        // for coreir generation
        bool create_json = false;
        uint8_t bitwidth;
        CoreIR::Context* context = NULL;
        CoreIR::Namespace* global_ns = NULL;
        std::map<std::string,CoreIR::Generator*> gens;
        CoreIR::ModuleDef* def = NULL;
        CoreIR::Module* design = NULL;
        CoreIR::Wireable* self = NULL;

        // keep track of coreir dag
        int input_idx = 0; // tracks how many inputs have been defined so far
        std::map<std::string,CoreIR::Wireable*> hw_wire_set;
        std::unordered_set<std::string> hw_inout_set;

        // coreir methods to wire things together
        virtual bool id_hw_input(const Expr e);
        bool id_cnst(const Expr e);
        int id_cnst_value(const Expr e);
        string id_hw_section(Expr a, Expr b, Type t, const char* op_symbol, string a_name, string b_name);
        CoreIR::Wireable* get_wire(Expr e, std::string name);

    };

    /** A name for the CoreIR target */
    std::string target_name;

    /** String streams for building header and source files. */
    // @{
    std::ostringstream hdr_stream;
    std::ostringstream src_stream;
    // @}

    /** Code generators for CoreIR target header and the source. */
    // @{
    CodeGen_CoreIR_C hdrc;
    CodeGen_CoreIR_C srcc;
    // @}    


};

}
}

#endif
