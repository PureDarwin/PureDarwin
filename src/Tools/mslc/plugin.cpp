// mslc clang plugin: MSL attributes ([[buffer(0)]], [[stage_in]] ...) and per entry function an annotate("E|...")
// descriptor that airify turns into the AIR entry function and its air.* metadata
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/RecordLayout.h"
#include "clang/Basic/DiagnosticSema.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Sema/ParsedAttr.h"
#include "clang/Sema/Sema.h"

#include <map>
#include <set>
#include <string>
#include <vector>

using namespace clang;

namespace {

// attribute texts per declaration, "buffer(0)", "user(locn0)", "position"
std::map<const Decl *, std::vector<std::string>> attrs;
// functions given a kernel attribute only so threadgroup locals are accepted
std::set<const FunctionDecl *> tg_kernels;

const char *const msl_names[] = {
  "buffer", "texture", "sampler", "threadgroup_buffer", "stage_in", "position", "user", "color", "attribute",
  "vertex_id", "instance_id", "base_vertex", "base_instance", "front_facing", "point_coord", "sample_id",
  "sample_mask", "depth", "point_size", "clip_distance", "flat", "center_perspective", "center_no_perspective",
  "centroid_perspective", "centroid_no_perspective", "sample_perspective", "sample_no_perspective",
  "thread_position_in_grid", "thread_position_in_threadgroup", "threadgroup_position_in_grid",
  "threads_per_threadgroup", "threadgroups_per_grid", "threads_per_grid", "thread_index_in_threadgroup",
  "thread_index_in_simdgroup", "simdgroup_index_in_threadgroup", "threads_per_simdgroup",
  "simdgroups_per_threadgroup", "thread_index_in_quadgroup", "quadgroup_index_in_threadgroup",
  "render_target_array_index", "viewport_array_index", "raster_order_group", "function_constant", "id",
  "msl_vertex", "msl_fragment", "msl_kernel", "msl_constant", "msl_threadgroup",
};

std::string attr_text(Sema &S, const ParsedAttr &A) {
  std::string t = A.getAttrName()->getName().str();

  if (!A.getNumArgs())
    return t;

  t += "(";
  for (unsigned i = 0; i < A.getNumArgs(); i++) {
    if (i)
      t += ",";
    if (A.isArgIdent(i)) {
      t += A.getArgAsIdent(i)->getIdentifierInfo()->getName().str();
      continue;
    }
    Expr *e = A.getArgAsExpr(i);
    Expr::EvalResult r;
    if (e && !e->isValueDependent() && e->EvaluateAsInt(r, S.Context))
      t += std::to_string(r.Val.getInt().getExtValue());
    else
      t += "?";
  }
  return t + ")";
}

struct MslAttr : public ParsedAttrInfo {
  std::vector<Spelling> sp;

  MslAttr() {
    OptArgs = 3;
    for (const char *n : msl_names) {
      sp.push_back({ ParsedAttr::AS_CXX11, n });
      sp.push_back({ ParsedAttr::AS_GNU, n });
    }
    Spellings = sp;
  }

  AttrHandling handleDeclAttribute(Sema &S, Decl *D, const ParsedAttr &A) const override {
    std::string t = attr_text(S, A);
    auto *VD = dyn_cast<VarDecl>(D);

    // program scope constants: __global (airify moves them to AIR's constant space 2), where __generic
    // methods and references still reach them
    if (t == "msl_constant") {
      if (VD && !isa<ParmVarDecl>(VD) && VD->isFileVarDecl()) {
        QualType q = S.Context.removeAddrSpaceQualType(VD->getType());
        VD->setType(S.Context.getAddrSpaceQualType(q, LangAS::opencl_global));
      }
      if (VD || isa<FieldDecl>(D))
        attrs[D].push_back("constant");
      return AttributeApplied;
    }

    // a threadgroup local: the enclosing function must look like a kernel to Sema's address space check
    if (t == "msl_threadgroup") {
      if (VD && !isa<ParmVarDecl>(VD) && VD->isLocalVarDecl()) {
        FunctionDecl *fd = S.getCurFunctionDecl();
        if (fd && !fd->hasAttr<DeviceKernelAttr>()) {
          fd->addAttr(DeviceKernelAttr::CreateImplicit(S.Context));
          tg_kernels.insert(fd);
        }
      }
      return AttributeApplied;
    }

    // a function constant: writable so it needs no initializer, airify gives it Apple's shape
    if (!t.compare(0, 18, "function_constant(") && VD && VD->isFileVarDecl()) {
      QualType q = VD->getType();
      QualType bare = S.Context.removeAddrSpaceQualType(q.getUnqualifiedType());
      VD->setType(S.Context.getAddrSpaceQualType(bare, LangAS::opencl_global));
      std::string fc = "FC|" + t.substr(18, t.size() - 19) + "|" + bare.getAsString(S.getPrintingPolicy()) + "|" +
                       VD->getNameAsString();
      VD->addAttr(AnnotateAttr::CreateImplicit(S.Context, fc, nullptr, 0));
    }

    attrs[D].push_back(t);
    return AttributeApplied;
  }
};

bool has_attr(const Decl *D, const std::string &prefix, std::string *out = nullptr) {
  auto it = attrs.find(D);

  if (it == attrs.end())
    return false;

  for (auto &s : it->second) {
    if (s.compare(0, prefix.size(), prefix) || (s.size() > prefix.size() && s[prefix.size()] != '('))
      continue;
    if (out)
      *out = s;
    return true;
  }
  return false;
}

std::string join_attrs(const Decl *D) {
  std::string r;
  auto it = attrs.find(D);

  if (it == attrs.end())
    return r;

  for (auto &s : it->second) {
    if (s == "constant" || !s.compare(0, 4, "msl_"))
      continue;
    if (!r.empty())
      r += ",";
    r += s;
  }
  return r;
}

// the int inside "name(n)"
unsigned attr_int(const std::string &s) {
  size_t p = s.find('(');
  return p == std::string::npos ? 0 : (unsigned)atoi(s.c_str() + p + 1);
}

struct Desc {
  ASTContext &ctx;
  PrintingPolicy pp;
  std::string out;

  Desc(ASTContext &c) : ctx(c), pp(c.getPrintingPolicy()) {
    pp.SuppressTagKeyword = true;
    pp.SuppressScope = false;
  }

  // the LLVM value type a scalar or vector becomes: f32 f16 s32 u32 s16 u16 s8 u8 b1, x lanes
  std::string lty(QualType t) {
    t = t.getCanonicalType();
    unsigned n = 0;

    if (auto *v = t->getAs<ExtVectorType>()) {
      n = v->getNumElements();
      t = v->getElementType().getCanonicalType();
    }

    std::string e = "?";
    if (auto *b = t->getAs<BuiltinType>()) {
      switch (b->getKind()) {
      case BuiltinType::Float: e = "f32"; break;
      case BuiltinType::Half: e = "f16"; break;
      case BuiltinType::Int: e = "s32"; break;
      case BuiltinType::UInt: e = "u32"; break;
      case BuiltinType::Short: e = "s16"; break;
      case BuiltinType::UShort: e = "u16"; break;
      case BuiltinType::Char_S:
      case BuiltinType::SChar: e = "s8"; break;
      case BuiltinType::Char_U:
      case BuiltinType::UChar: e = "u8"; break;
      case BuiltinType::Bool: e = "b1"; break;
      case BuiltinType::Long: e = "s64"; break;
      case BuiltinType::ULong: e = "u64"; break;
      default: break;
      }
    }
    return n ? e + "x" + std::to_string(n) : e;
  }

  std::string tname(QualType t) {
    t = ctx.removeAddrSpaceQualType(t.getUnqualifiedType());
    // scalars by their MSL names whatever typedef spelled them (uint32_t is uint), as Apple's type names
    if (t->isBuiltinType()) {
      static const char *const msl[][2] = { { "unsigned int", "uint" }, { "unsigned short", "ushort" },
                                            { "unsigned char", "uchar" }, { "unsigned long", "ulong" },
                                            { "signed char", "char" }, { "_Bool", "bool" } };
      std::string c = t.getCanonicalType().getAsString(pp);
      for (auto &m : msl) {
        if (c == m[0])
          return m[1];
      }
      return c;
    }
    return t.getAsString(pp);
  }

  // texture2d<float, sample>: the class template's element type and access
  std::string texture_name(const ClassTemplateSpecializationDecl *cs, unsigned *acc) {
    static const char *const names[] = { "sample", "read", "write", "read_write" };
    const TemplateArgumentList &ta = cs->getTemplateArgs();
    unsigned a = ta.size() > 1 ? (unsigned)ta[1].getAsIntegral().getZExtValue() : 0;

    *acc = a;
    return cs->getName().str() + "<" + tname(ta[0].getAsType()) + ", " + names[a & 3] + ">";
  }

  void fields(const RecordDecl *rd, char tag) {
    const ASTRecordLayout &l = ctx.getASTRecordLayout(rd);

    for (const FieldDecl *f : rd->fields()) {
      uint64_t off = l.getFieldOffset(f->getFieldIndex()) / 8;
      QualType ft = f->getType();
      std::string a = join_attrs(f);

      if (tag == 'S' || tag == 'T') {
        // buffer struct info: offset, element size, array length, type, name, and for an argument buffer
        // member ([[id(n)]]) what it holds
        uint64_t count = 0;
        QualType et = ft;
        std::string id, ind;
        if (auto *at = ctx.getAsConstantArrayType(ft))
          count = at->getSize().getZExtValue(), et = at->getElementType();
        std::string etn = tname(et);
        const RecordDecl *inner = nullptr;
        if (tag == 'S' && has_attr(f, "id", &id))
          ind = indirect(f, et, attr_int(id), &etn, &inner);
        out += std::string(1, tag) + "|" + std::to_string(off) + "|" +
               std::to_string(ctx.getTypeSizeInChars(et).getQuantity()) + "|" + std::to_string(count) + "|" + etn +
               "|" + f->getNameAsString() + "|" + ind + "\n";
        if (inner)
          fields(inner, 'T');
        continue;
      }
      out += std::string(1, tag) + "|" + std::to_string(off) + "|" + lty(ft) + "|" + tname(ft) + "|" + a + "|" +
             f->getNameAsString() + "\n";
    }
  }

  // an argument buffer member: kind;id;address space;access;size;align;type name
  std::string indirect(const FieldDecl *f, QualType t, unsigned id, std::string *etn, const RecordDecl **inner) {
    std::string ids = std::to_string(id);

    if (t->isPointerType()) {
      QualType pt = t->getPointeeType();
      unsigned as = pt.getAddressSpace() == LangAS::opencl_local ? 3 : has_attr(f, "constant") ? 2 : 1;
      QualType vt = ctx.removeAddrSpaceQualType(pt.getUnqualifiedType());
      bool sized = !vt->isIncompleteType() && !vt->isVoidType();
      *etn = tname(vt);
      if (auto *rd = vt->getAsRecordDecl(); rd && sized)
        *inner = rd;
      return "buffer;" + ids + ";" + std::to_string(as) + ";" +
             (pt.isConstQualified() || as == 2 ? "read" : "read_write") + ";" +
             (sized ? std::to_string(ctx.getTypeSizeInChars(vt).getQuantity()) : "") + ";" +
             (sized ? std::to_string(ctx.getTypeAlignInChars(vt).getQuantity()) : "") + ";" + *etn;
    }
    if (auto *cs = dyn_cast_or_null<ClassTemplateSpecializationDecl>(t->getAsRecordDecl())) {
      static const char *const acc[] = { "sample", "read", "write", "read_write" };
      unsigned a = 0;
      *etn = texture_name(cs, &a);
      return "texture;" + ids + ";;" + acc[a & 3] + ";;;" + *etn;
    }
    if (tname(t) == "sampler" || tname(t) == "metal::sampler")
      return "sampler;" + ids + ";;;;;sampler";
    return "";
  }

  void param(unsigned i, const ParmVarDecl *p) {
    QualType t = p->getType();
    std::string a, name = p->getNameAsString(), is = std::to_string(i);

    if (has_attr(p, "buffer", &a) || has_attr(p, "threadgroup_buffer", &a)) {
      bool ref = t->isReferenceType();
      QualType pt = t->getPointeeType();
      LangAS as = pt.getAddressSpace();
      unsigned air_as = as == LangAS::opencl_local ? 3 : has_attr(p, "constant") ? 2 : 1;
      const char *acc = pt.isConstQualified() || air_as == 2 ? "read" : "read_write";
      QualType vt = ctx.removeAddrSpaceQualType(pt.getUnqualifiedType());
      uint64_t sz = vt->isIncompleteType() ? 0 : ctx.getTypeSizeInChars(vt).getQuantity();
      uint64_t al = vt->isIncompleteType() ? 0 : ctx.getTypeAlignInChars(vt).getQuantity();

      out += "P|" + is + "|buffer|" + std::to_string(attr_int(a)) + "|" + std::to_string(air_as) + "|" + acc + "|" +
             (ref ? "ref" : "ptr") + "|" + std::to_string(sz) + "|" + std::to_string(al) + "|" + tname(vt) + "|" +
             name + "\n";
      if (auto *rd = vt->getAsRecordDecl(); rd && !isa<ClassTemplateSpecializationDecl>(rd))
        fields(rd, 'S');
      return;
    }

    if (has_attr(p, "texture", &a)) {
      auto *cs = dyn_cast_or_null<ClassTemplateSpecializationDecl>(t->getAsRecordDecl());
      static const char *const acc[] = { "sample", "read", "write", "read_write" };
      unsigned ac = 0;
      std::string tn = cs ? texture_name(cs, &ac) : tname(t);
      out += "P|" + is + "|texture|" + std::to_string(attr_int(a)) + "|" + tn + "|" + acc[ac & 3] + "|" + name + "\n";
      return;
    }

    if (has_attr(p, "sampler", &a)) {
      out += "P|" + is + "|sampler|" + std::to_string(attr_int(a)) + "|" + name + "\n";
      return;
    }

    if (has_attr(p, "stage_in")) {
      const RecordDecl *rd = t->getAsRecordDecl();
      out += "P|" + is + "|stage_in|" + tname(t) + "|" + std::to_string(ctx.getTypeSizeInChars(t).getQuantity()) +
             "|" + name + "\n";
      if (rd)
        fields(rd, 'F');
      return;
    }

    // a builtin value: its attribute names it
    out += "P|" + is + "|builtin|" + join_attrs(p) + "|" + lty(t) + "|" + tname(t) + "|" + name + "\n";
  }

  void entry(const FunctionDecl *fd, const std::string &stage) {
    QualType rt = fd->getReturnType();

    out = "E|" + stage + "|" + fd->getNameAsString() + "\n";
    if (rt->isVoidType()) {
      out += "R|void\n";
    } else if (const RecordDecl *rd = rt->getAsRecordDecl()) {
      out += "R|struct|" + std::to_string(ctx.getTypeSizeInChars(rt).getQuantity()) + "\n";
      fields(rd, 'O');
    } else {
      out += "R|value|" + lty(rt) + "|" + tname(rt) + "|" + join_attrs(fd) + "\n";
    }

    for (unsigned i = 0; i < fd->getNumParams(); i++) {
      param(i, fd->getParamDecl(i));
    }
  }
};

class MslConsumer : public ASTConsumer {
public:
  bool HandleTopLevelDecl(DeclGroupRef dg) override {
    for (Decl *d : dg) {
      auto *fd = dyn_cast<FunctionDecl>(d);
      std::string st;

      if (!fd || !fd->doesThisDeclarationHaveABody())
        continue;

      // the kernel attribute only got threadgroup locals past Sema: codegen must see a plain function
      if (tg_kernels.count(fd))
        fd->dropAttr<DeviceKernelAttr>();
      if (has_attr(fd, "msl_vertex"))
        st = "vertex";
      else if (has_attr(fd, "msl_fragment"))
        st = "fragment";
      else if (has_attr(fd, "msl_kernel"))
        st = "kernel";
      else
        continue;

      Desc ds(fd->getASTContext());
      ds.entry(fd, st);
      fd->addAttr(AnnotateAttr::CreateImplicit(fd->getASTContext(), ds.out, nullptr, 0));
    }
    return true;
  }
};

class MslAction : public PluginASTAction {
protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &ci, llvm::StringRef) override {
    // MSL's unsuffixed float literals are single precision anyway
    ci.getDiagnostics().setSeverity(diag::warn_double_const_requires_fp64, diag::Severity::Ignored, SourceLocation());
    return std::make_unique<MslConsumer>();
  }
  bool ParseArgs(const CompilerInstance &, const std::vector<std::string> &) override {
    return true;
  }
  ActionType getActionType() override {
    return AddBeforeMainAction;
  }
};

} // namespace

static ParsedAttrInfoRegistry::Add<MslAttr> A("msl", "Metal Shading Language attributes");
static FrontendPluginRegistry::Add<MslAction> B("msl", "Metal Shading Language entry descriptors");
