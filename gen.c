#include "aqcc.h"

typedef struct {
    int nlabel, loop_continue_label, loop_break_label;
    Vector *codes;
} CodeEnv;

CodeEnv *new_code_env()
{
    CodeEnv *this;

    this = (CodeEnv *)safe_malloc(sizeof(CodeEnv));
    this->nlabel = 0;
    this->loop_continue_label = this->loop_break_label = -1;
    this->codes = new_vector();
    return this;
}

Vector *swap_codes(CodeEnv *env, Vector *new_codes)
{
    Vector *ret = env->codes;
    env->codes = new_codes;
    return ret;
}

void appcode(Vector *codes, const char *src, ...)
{
    char buf[256], buf2[256];  // TODO: enoguth length?
    int i, bufidx;
    va_list args;

    // copy src to buf.
    // replace # to %% in src.
    for (i = 0, bufidx = 0;; i++) {
        if (src[i] == '\0') break;

        if (src[i] == '#') {
            buf[bufidx++] = '%';
            buf[bufidx++] = '%';
            continue;
        }

        buf[bufidx++] = src[i];
    }
    buf[bufidx] = '\0';

    va_start(args, src);
    vsprintf(buf2, buf, args);
    va_end(args);

    vector_push_back(codes, new_str(buf2));
}

void dump_codes(Vector *codes, FILE *fh)
{
    int i;

    for (i = 0; i < vector_size(codes); i++)
        fprintf(fh, "%s\n", (const char *)vector_get(codes, i));
}

void generate_code_detail(CodeEnv *env, AST *ast)
{
    switch (ast->kind) {
        case AST_ADD:
            generate_code_detail(env, ast->lhs);
            generate_code_detail(env, ast->rhs);
            appcode(env->codes, "pop #rdi");
            appcode(env->codes, "pop #rax");

            // int + ptr
            if (match_type2(ast->lhs, ast->rhs, TY_INT, TY_PTR)) {
                appcode(env->codes, "cltq");  // TODO: long
                appcode(env->codes, "imul $%d, #rax",
                        ast->rhs->type->ptr_of->nbytes);
            }

            appcode(env->codes, "add %s, %s", reg_name(ast->type->nbytes, 1),
                    reg_name(ast->type->nbytes, 0));
            appcode(env->codes, "push #rax");
            break;

        case AST_SUB: {
            int nbytes;

            generate_code_detail(env, ast->lhs);
            generate_code_detail(env, ast->rhs);

            appcode(env->codes, "pop #rax");
            appcode(env->codes, "pop #rdi");

            // ptr - int
            if (match_type2(ast->lhs, ast->rhs, TY_PTR, TY_INT)) {
                appcode(env->codes, "cltq");  // TODO: long
                appcode(env->codes, "imul $%d, #rax",
                        ast->lhs->type->ptr_of->nbytes);
            }

            nbytes = ast->lhs->type->nbytes;
            appcode(env->codes, "sub %s, %s", reg_name(nbytes, 0),
                    reg_name(nbytes, 1));

            // ptr - ptr
            if (match_type2(ast->lhs, ast->rhs, TY_PTR, TY_PTR))
                // TODO: assume pointer size is 8.
                appcode(env->codes, "sar $%d, #rdi", 2);

            appcode(env->codes, "push #rdi");
        } break;

        case AST_MUL:
            generate_code_detail(env, ast->lhs);
            generate_code_detail(env, ast->rhs);
            appcode(env->codes, "pop #rdi");
            appcode(env->codes, "pop #rax");
            appcode(env->codes, "imul #edi, #eax");
            appcode(env->codes, "push #rax");
            break;

        case AST_DIV:
            generate_code_detail(env, ast->lhs);
            generate_code_detail(env, ast->rhs);
            appcode(env->codes, "pop #rdi");
            appcode(env->codes, "pop #rax");
            appcode(env->codes, "cltd");
            appcode(env->codes, "idiv #edi");
            appcode(env->codes, "push #rax");
            break;

        case AST_REM:
            generate_code_detail(env, ast->lhs);
            generate_code_detail(env, ast->rhs);
            appcode(env->codes, "pop #rdi");
            appcode(env->codes, "pop #rax");
            appcode(env->codes, "cltd");
            appcode(env->codes, "idiv #edi");
            appcode(env->codes, "push #rdx");
            break;

        case AST_UNARY_MINUS:
            generate_code_detail(env, ast->lhs);
            appcode(env->codes, "pop #rax");
            appcode(env->codes, "neg #eax");
            appcode(env->codes, "push #rax");
            break;

        case AST_LSHIFT:
            generate_code_detail(env, ast->lhs);
            generate_code_detail(env, ast->rhs);
            appcode(env->codes, "pop #rcx");
            appcode(env->codes, "pop #rax");
            appcode(env->codes, "sal #cl, #eax");
            appcode(env->codes, "push #rax");
            break;

        case AST_RSHIFT:
            generate_code_detail(env, ast->lhs);
            generate_code_detail(env, ast->rhs);
            appcode(env->codes, "pop #rcx");
            appcode(env->codes, "pop #rax");
            appcode(env->codes, "sar #cl, #eax");
            appcode(env->codes, "push #rax");
            break;

        case AST_LT:
            generate_code_detail(env, ast->lhs);
            generate_code_detail(env, ast->rhs);
            appcode(env->codes, "pop #rdi");
            appcode(env->codes, "pop #rax");
            appcode(env->codes, "cmp #edi, #eax");
            appcode(env->codes, "setl #al");
            appcode(env->codes, "movzb #al, #eax");
            appcode(env->codes, "push #rax");
            break;

        case AST_GT:
            generate_code_detail(env, ast->lhs);
            generate_code_detail(env, ast->rhs);
            appcode(env->codes, "pop #rdi");
            appcode(env->codes, "pop #rax");
            appcode(env->codes, "cmp #edi, #eax");
            appcode(env->codes, "setg #al");
            appcode(env->codes, "movzb #al, #eax");
            appcode(env->codes, "push #rax");
            break;

        case AST_LTE:
            generate_code_detail(env, ast->lhs);
            generate_code_detail(env, ast->rhs);
            appcode(env->codes, "pop #rdi");
            appcode(env->codes, "pop #rax");
            appcode(env->codes, "cmp #edi, #eax");
            appcode(env->codes, "setle #al");
            appcode(env->codes, "movzb #al, #eax");
            appcode(env->codes, "push #rax");
            break;

        case AST_GTE:
            generate_code_detail(env, ast->lhs);
            generate_code_detail(env, ast->rhs);
            appcode(env->codes, "pop #rdi");
            appcode(env->codes, "pop #rax");
            appcode(env->codes, "cmp #edi, #eax");
            appcode(env->codes, "setge #al");
            appcode(env->codes, "movzb #al, #eax");
            appcode(env->codes, "push #rax");
            break;

        case AST_EQ:
            generate_code_detail(env, ast->lhs);
            generate_code_detail(env, ast->rhs);
            appcode(env->codes, "pop #rdi");
            appcode(env->codes, "pop #rax");
            appcode(env->codes, "cmp #edi, #eax");
            appcode(env->codes, "sete #al");
            appcode(env->codes, "movzb #al, #eax");
            appcode(env->codes, "push #rax");
            break;

        case AST_NEQ:
            generate_code_detail(env, ast->lhs);
            generate_code_detail(env, ast->rhs);
            appcode(env->codes, "pop #rdi");
            appcode(env->codes, "pop #rax");
            appcode(env->codes, "cmp #edi, #eax");
            appcode(env->codes, "setne #al");
            appcode(env->codes, "movzb #al, #eax");
            appcode(env->codes, "push #rax");
            break;

        case AST_AND:
            generate_code_detail(env, ast->lhs);
            generate_code_detail(env, ast->rhs);
            appcode(env->codes, "pop #rdi");
            appcode(env->codes, "pop #rax");
            appcode(env->codes, "and #edi, #eax");
            appcode(env->codes, "push #rax");
            break;

        case AST_XOR:
            generate_code_detail(env, ast->lhs);
            generate_code_detail(env, ast->rhs);
            appcode(env->codes, "pop #rdi");
            appcode(env->codes, "pop #rax");
            appcode(env->codes, "xor #edi, #eax");
            appcode(env->codes, "push #rax");
            break;

        case AST_OR:
            generate_code_detail(env, ast->lhs);
            generate_code_detail(env, ast->rhs);
            appcode(env->codes, "pop #rdi");
            appcode(env->codes, "pop #rax");
            appcode(env->codes, "or #edi, #eax");
            appcode(env->codes, "push #rax");
            break;

        case AST_LAND: {
            int false_label = env->nlabel++, exit_label = env->nlabel++;
            generate_code_detail(env, ast->lhs);
            appcode(env->codes, "pop #rax");
            appcode(env->codes, "cmp $0, #eax");
            appcode(env->codes, "je .L%d", false_label);
            // don't execute rhs expression if lhs is false.
            generate_code_detail(env, ast->rhs);
            appcode(env->codes, "pop #rax");
            appcode(env->codes, "cmp $0, #eax");
            appcode(env->codes, "je .L%d", false_label);
            appcode(env->codes, "mov $1, #eax");
            appcode(env->codes, "jmp .L%d", exit_label);
            appcode(env->codes, ".L%d:", false_label);
            appcode(env->codes, "mov $0, #eax");
            appcode(env->codes, ".L%d:", exit_label);
            appcode(env->codes, "push #rax");
        } break;

        case AST_LOR: {
            int true_label = env->nlabel++, exit_label = env->nlabel++;
            generate_code_detail(env, ast->lhs);
            appcode(env->codes, "pop #rax");
            appcode(env->codes, "cmp $0, #eax");
            appcode(env->codes, "jne .L%d", true_label);
            // don't execute rhs expression if lhs is true.
            generate_code_detail(env, ast->rhs);
            appcode(env->codes, "pop #rax");
            appcode(env->codes, "cmp $0, #eax");
            appcode(env->codes, "jne .L%d", true_label);
            appcode(env->codes, "mov $0, #eax");
            appcode(env->codes, "jmp .L%d", exit_label);
            appcode(env->codes, ".L%d:", true_label);
            appcode(env->codes, "mov $1, #eax");
            appcode(env->codes, ".L%d:", exit_label);
            appcode(env->codes, "push #rax");
        } break;

        case AST_POSTINC: {
            appcode(env->codes, "push %d(#rbp)", ast->lhs->stack_idx);
            if (match_type(ast->lhs, TY_PTR))
                appcode(env->codes, "add $%d, %d(#rbp)",
                        ast->lhs->type->ptr_of->nbytes, ast->lhs->stack_idx);
            else
                appcode(env->codes, "incl %d(#rbp)", ast->lhs->stack_idx);
        } break;

        case AST_PREINC: {
            if (match_type(ast->lhs, TY_PTR))
                appcode(env->codes, "add $%d, %d(#rbp)",
                        ast->lhs->type->ptr_of->nbytes, ast->lhs->stack_idx);
            else
                appcode(env->codes, "incl %d(#rbp)", ast->lhs->stack_idx);
            appcode(env->codes, "push %d(#rbp)", ast->lhs->stack_idx);
        } break;

        case AST_ADDR: {
            appcode(env->codes, "lea %d(#rbp), #rax", ast->lhs->stack_idx);
            appcode(env->codes, "push %rax");
        } break;

        case AST_INDIR: {
            generate_code_detail(env, ast->lhs);
            appcode(env->codes, "pop %rax");
            appcode(env->codes, "mov (%rax), #rdi");
            appcode(env->codes, "push %rdi");
        } break;

        case AST_IF:
        case AST_COND: {
            int false_label = env->nlabel++, exit_label = env->nlabel++;

            generate_code_detail(env, ast->cond);
            appcode(env->codes, "pop #rax");
            appcode(env->codes, "cmp $0, #eax");
            appcode(env->codes, "je .L%d", false_label);
            generate_code_detail(env, ast->then);
            appcode(env->codes, "jmp .L%d", exit_label);
            appcode(env->codes, ".L%d:", false_label);
            if (ast->els != NULL) generate_code_detail(env, ast->els);
            appcode(env->codes, ".L%d:", exit_label);
        } break;

        case AST_ASSIGN: {
            generate_code_detail(env, ast->rhs);

            if (ast->lhs->kind == AST_INDIR) {
                // should be treated as lvalue.
                generate_code_detail(env, ast->lhs->lhs);

                appcode(env->codes, "pop #rdi");
                appcode(env->codes, "pop #rax");
                appcode(env->codes, "mov #rax, (#rdi)");
                appcode(env->codes, "push #rax");
                break;
            }

            if (ast->lhs->kind == AST_LVAR) {
                assert(ast->lhs->kind == AST_LVAR);
                appcode(env->codes, "pop #rax");
                appcode(env->codes, "mov %s, %d(#rbp)",
                        reg_name(ast->lhs->type->nbytes, 0),
                        ast->lhs->stack_idx);
                appcode(env->codes, "push #rax");
                break;
            }

            assert(0);
        } break;

        case AST_LVAR:
            appcode(env->codes, "push %d(#rbp)", ast->stack_idx);
            break;

        case AST_FUNCCALL: {
            int i;

            for (i = vector_size(ast->args) - 1; i >= 0; i--) {
                generate_code_detail(env, (AST *)(vector_get(ast->args, i)));
                if (i < 6) appcode(env->codes, "pop %s", reg_name(8, i + 1));
            }
            appcode(env->codes, "call %s", ast->fname);
            appcode(env->codes, "push #rax");
        } break;

        case AST_FUNCDEF: {
            int i, stack_idx;

            // allocate stack
            stack_idx = 0;
            for (i = max(0, vector_size(ast->params) - 6);
                 i < vector_size(ast->env->scoped_vars); i++) {
                AST *var = (AST *)(vector_get(ast->env->scoped_vars, i));

                stack_idx -= var->type->nbytes;
                var->stack_idx = stack_idx;
            }

            // generate code
            appcode(env->codes, "%s:", ast->fname);
            appcode(env->codes, "push #rbp");
            appcode(env->codes, "mov #rsp, #rbp");
            appcode(env->codes, "sub $%d, #rsp",
                    (int)(ceil(-stack_idx / 16.)) * 16);

            // assign param to localvar
            for (i = 0; i < vector_size(ast->params); i++) {
                AST *var = lookup_var(
                    ast->env,
                    (const char *)(((Pair *)vector_get(ast->params, i))
                                       ->second));
                if (i < 6)
                    appcode(env->codes, "mov %s, %d(#rbp)",
                            reg_name(var->type->nbytes, i + 1), var->stack_idx);
                else
                    // should avoid return pointer and saved %rbp
                    var->stack_idx = 16 + (i - 6) * 8;
            }

            // generate body
            generate_code_detail(env, ast->body);

            // avoid duplicate needless `ret`
            if (strcmp((const char *)vector_get(env->codes,
                                                vector_size(env->codes) - 1),
                       "ret") == 0)
                break;
            appcode(env->codes, "mov $0, #eax");
            appcode(env->codes, "mov #rbp, #rsp");
            appcode(env->codes, "pop #rbp");
            appcode(env->codes, "ret");
        } break;

        case AST_EXPR_STMT:
            if (ast->lhs == NULL) break;
            generate_code_detail(env, ast->lhs);
            appcode(env->codes, "pop #rax");
            break;

        case AST_RETURN:
            if (ast->lhs == NULL) {
                appcode(env->codes, "mov $0, #eax");
            }
            else {
                generate_code_detail(env, ast->lhs);
                appcode(env->codes, "pop #rax");
            }
            appcode(env->codes, "mov #rbp, #rsp");
            appcode(env->codes, "pop #rbp");
            appcode(env->codes, "ret");
            break;

        case AST_WHILE: {
            int org_loop_start_label = env->loop_continue_label,
                org_loop_end_label = env->loop_break_label;
            env->loop_continue_label = env->nlabel++;
            env->loop_break_label = env->nlabel++;

            appcode(env->codes, ".L%d:", env->loop_continue_label);
            assert(ast->cond != NULL);
            generate_code_detail(env, ast->cond);
            appcode(env->codes, "pop #rax");
            appcode(env->codes, "cmp $0, #eax");
            appcode(env->codes, "je .L%d", env->loop_break_label);
            generate_code_detail(env, ast->then);
            appcode(env->codes, "jmp .L%d", env->loop_continue_label);
            appcode(env->codes, ".L%d:", env->loop_break_label);

            env->loop_continue_label = org_loop_start_label;
            env->loop_break_label = org_loop_end_label;
        } break;

        case AST_FOR: {
            int org_loop_start_label = env->loop_continue_label,
                org_loop_end_label = env->loop_break_label;
            int loop_start_label = env->nlabel++;
            env->loop_continue_label = env->nlabel++;
            env->loop_break_label = env->nlabel++;

            if (ast->initer != NULL) {
                generate_code_detail(env, ast->initer);
                appcode(env->codes, "pop #rax");
            }
            appcode(env->codes, ".L%d:", loop_start_label);
            if (ast->midcond != NULL) {
                generate_code_detail(env, ast->midcond);
                appcode(env->codes, "pop #rax");
                appcode(env->codes, "cmp $0, #eax");
                appcode(env->codes, "je .L%d", env->loop_break_label);
            }
            generate_code_detail(env, ast->for_body);
            appcode(env->codes, ".L%d:", env->loop_continue_label);
            if (ast->iterer != NULL) {
                generate_code_detail(env, ast->iterer);
                appcode(env->codes, "pop #rax");
            }
            appcode(env->codes, "jmp .L%d", loop_start_label);
            appcode(env->codes, ".L%d:", env->loop_break_label);

            env->loop_continue_label = org_loop_start_label;
            env->loop_break_label = org_loop_end_label;
        } break;

        case AST_BREAK:
            if (env->loop_break_label < 0)
                error("invalid break.", __FILE__, __LINE__);
            appcode(env->codes, "jmp .L%d", env->loop_break_label);
            break;

        case AST_CONTINUE:
            if (env->loop_continue_label < 0)
                error("invalid continue.", __FILE__, __LINE__);
            appcode(env->codes, "jmp .L%d", env->loop_continue_label);
            break;

        case AST_COMPOUND: {
            int i;

            for (i = 0; i < vector_size(ast->stmts); i++)
                generate_code_detail(env, (AST *)vector_get(ast->stmts, i));
        } break;

        case AST_INT:
            appcode(env->codes, "mov $%d, #eax", ast->ival);
            appcode(env->codes, "push #rax");
            break;

        case AST_NOP:
            break;

        case AST_ARY2PTR:
            if (ast->ary->kind == AST_LVAR) {
                appcode(env->codes, "lea %d(#rbp), #rax", ast->ary->stack_idx);
                appcode(env->codes, "push #rax");
            }
            else {
                // TODO: always indirection?
                assert(ast->ary->kind == AST_INDIR);
                generate_code_detail(env, ast->ary->lhs);
            }
            break;

        default:
            assert(0);
    }
}

Vector *generate_code(Vector *asts)
{
    int i;
    CodeEnv *env;

    env = new_code_env();

    appcode(env->codes, ".global main");

    for (i = 0; i < vector_size(asts); i++)
        generate_code_detail(env, (AST *)vector_get(asts, i));

    return env->codes;
}
