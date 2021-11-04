#include "statement.h"

#include <iostream>
#include <sstream>

namespace ast
  {
    using namespace std::literals;

    using runtime::Closure;
    using runtime::Context;
    using runtime::ObjectHolder;

    namespace
      {
        const std::string ADD_METHOD = "__add__"s;
        const std::string INIT_METHOD = "__init__"s;
       } // namespace

    class RuntimeReturnExeption;

    VariableValue::VariableValue(const std::string &var_name) {
      dotted_ids_.push_back(var_name);
    }

    VariableValue::VariableValue(std::vector<std::string> dotted_ids)
        : dotted_ids_(std::move(dotted_ids)) {
    }

    ObjectHolder VariableValue::Execute(Closure &closure, Context & /* context */) {
      Closure *closure_ptr = &closure;
      for (size_t i = 0; i < dotted_ids_.size(); ++i) {
        const std::string &field_name = dotted_ids_[i];
        if (closure_ptr->count(field_name) == 0) {
          throw std::runtime_error("Cant find var"s);
        }
        if (i == dotted_ids_.size() - 1) {
          return closure_ptr->at(field_name);
        }
        auto ptr_obj = closure_ptr->at(field_name).TryAs<runtime::ClassInstance>();
        if (!ptr_obj) {
          throw std::runtime_error("This isn't object"s);
        }
        closure_ptr = &ptr_obj->Fields();
      }
      return {};
    }

    Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv)
        : var_(std::move(var))
        , rv_(std::move(rv)) {
    }

    ObjectHolder Assignment::Execute(Closure &closure, Context &context) {
      closure[var_] = rv_->Execute(closure, context);
      return closure[var_];
    }

    FieldAssignment::FieldAssignment(VariableValue object, std::string field_name, std::unique_ptr<Statement> rv)
        : object_(std::move(object))
        , field_name_(std::move(field_name))
        , rv_(std::move(rv)) {
    }

    ObjectHolder FieldAssignment::Execute(Closure &closure, Context &context) {
      const auto obj = object_.Execute(closure, context);
      const auto class_inst_ptr = obj.TryAs<runtime::ClassInstance>();
      if (class_inst_ptr) {
        class_inst_ptr->Fields()[field_name_] = rv_->Execute(closure, context);
        return class_inst_ptr->Fields()[field_name_];
      }
      throw std::runtime_error("Cant find field"s);
    }

    NewInstance::NewInstance(const runtime::Class &class_)
        : cls_(class_) {
    }

    NewInstance::NewInstance(const runtime::Class &class_, std::vector<std::unique_ptr<Statement>> args)
        : cls_(class_)
        , args_(std::move(args)) {
    }

    ObjectHolder NewInstance::Execute(Closure &closure, Context &context) {
      std::vector<runtime::ObjectHolder> actual_args;
      for (const auto& arg : args_) {
        actual_args.push_back(arg->Execute(closure, context));
      }
      if (cls_.HasMethod(INIT_METHOD, args_.size())) {
        cls_.Call(INIT_METHOD, actual_args, context);
      }
      return runtime::ObjectHolder::Own(runtime::ClassInstance{cls_});
    }

    MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method_name,
                           std::vector<std::unique_ptr<Statement>> args)
        : object_(std::move(object))
        , method_name_(std::move(method_name))
        , args_(std::move(args)) {
    }

    ObjectHolder MethodCall::Execute(Closure &closure, Context &context) {
      const auto obj = object_->Execute(closure, context);
      const auto class_instance_ptr = obj.TryAs<runtime::ClassInstance>();
      if (obj && class_instance_ptr && class_instance_ptr->HasMethod(method_name_, args_.size())) {
        std::vector<runtime::ObjectHolder> actual_args;
        for (const auto &arg: args_) {
          actual_args.push_back(arg->Execute(closure, context));
        }
        return class_instance_ptr->Call(method_name_, actual_args, context);
      }
      return {};
    }

    void Compound::AddStatement(std::unique_ptr<Statement> stmt) {
      statements_.push_back(std::move(stmt));
    }

    ObjectHolder Compound::Execute(Closure &closure, Context &context) {
      for (const auto &statement: statements_) {
        statement->Execute(closure, context);
      }
      return {};
    }

    class RuntimeReturnExeption
        : public std::exception {
     public:
      explicit RuntimeReturnExeption(const runtime::ObjectHolder &obj)
          : obj_(obj) {
      }

      runtime::ObjectHolder GetValue() {
        return obj_;
      }

     private:
      runtime::ObjectHolder obj_;
    };

    Return::Return(std::unique_ptr<Statement> statement)
        : statement_(std::move(statement)) {
    }

    ObjectHolder Return::Execute(Closure &closure, Context &context) {
      throw RuntimeReturnExeption(statement_->Execute(closure, context));
    }

    MethodBody::MethodBody(std::unique_ptr<Statement> &&body)
        : body_(std::move(body)) {
    }

    ObjectHolder MethodBody::Execute(Closure &closure, Context &context) {

      try {
        body_->Execute(closure, context);
      } catch (RuntimeReturnExeption &runtimeReturnExeption) {
        return runtimeReturnExeption.GetValue();
      }

      return {};
    }

    ClassDefinition::ClassDefinition(ObjectHolder cls)
        : cls_(cls) {
    }

    ObjectHolder ClassDefinition::Execute(Closure &closure, Context & /* context */) {
      const auto obj = cls_.TryAs<runtime::Class>();
      closure[obj->GetName()] = std::move(cls_);
      return {};
    }

    Print::Print(std::unique_ptr<Statement> argument) {
      args_.push_back(std::move(argument));
    }

    Print::Print(std::vector<std::unique_ptr<Statement>> args)
        : args_(std::move(args)) {
    }

    std::unique_ptr<Print> Print::Variable(const std::string &name) {
      return std::make_unique<Print>(std::make_unique<VariableValue>(name));
    }

    ObjectHolder Print::Execute(Closure &closure, Context &context) {
      ObjectHolder obj;
      bool first_arg = true;
      for (const auto &arg: args_) {
        if (!first_arg) {
          context.GetOutputStream() << " "s;
        }
        first_arg = false;
        obj = arg->Execute(closure, context);
        if (obj) {
          obj->Print(context.GetOutputStream(), context);
        } else {
          context.GetOutputStream() << "None"s;
        }
      }
      context.GetOutputStream() << "\n"s;
      return {};
    }

    ObjectHolder Stringify::Execute(Closure &closure, Context &context) {
      auto obj = argument_->Execute(closure, context);
      if (!obj) {
        return ObjectHolder::Own(runtime::String{"None"s});
      }
      runtime::DummyContext dummy_context;
      obj->Print(dummy_context.output, dummy_context);
      return ObjectHolder::Own(runtime::String{dummy_context.output.str()});
    }

    ObjectHolder Add::Execute(Closure &closure, Context &context) {
      if (!rhs_ || !lhs_) {
        throw std::runtime_error("null operands are not supported"s);
      }
      const auto obj_lhs = lhs_->Execute(closure, context);
      const auto obj_rhs = rhs_->Execute(closure, context);
      {
        const auto ptr_lhs_n = obj_lhs.TryAs<runtime::Number>();
        const auto ptr_rhs_n = obj_rhs.TryAs<runtime::Number>();

        if (ptr_lhs_n != nullptr && ptr_rhs_n != nullptr) {
          const auto l_num = ptr_lhs_n->GetValue();
          const auto r_num = ptr_rhs_n->GetValue();

          return ObjectHolder::Own(runtime::Number{l_num + r_num});
        }
      }
      {
        const auto ptr_lhs_s = obj_lhs.TryAs<runtime::String>();
        const auto ptr_rhs_s = obj_rhs.TryAs<runtime::String>();

        if (ptr_lhs_s != nullptr && ptr_rhs_s != nullptr) {
          const auto l_str = ptr_lhs_s->GetValue();
          const auto r_str = ptr_rhs_s->GetValue();

          return ObjectHolder::Own(runtime::String{l_str + r_str});
        }
      }
      auto ptr_lhs_class_inst = obj_lhs.TryAs<runtime::ClassInstance>();

      if (ptr_lhs_class_inst != nullptr) {
        if (ptr_lhs_class_inst->HasMethod(ADD_METHOD, 1)) {
          return ptr_lhs_class_inst->Call(ADD_METHOD, {obj_rhs}, context);
        }
      }

      throw std::runtime_error("incorrect add operands"s);
    }

    ObjectHolder Sub::Execute(Closure &closure, Context &context) {
      if (!rhs_ || !lhs_) {
        throw std::runtime_error("null operands are not supported"s);
      }

      const auto obj_lhs = lhs_->Execute(closure, context);
      const auto obj_rhs = rhs_->Execute(closure, context);

      const auto ptr_lhs_n = obj_lhs.TryAs<runtime::Number>();
      const auto ptr_rhs_n = obj_rhs.TryAs<runtime::Number>();

      if (ptr_lhs_n != nullptr && ptr_rhs_n != nullptr) {
        auto l_num = ptr_lhs_n->GetValue();
        auto r_num = ptr_rhs_n->GetValue();

        return ObjectHolder::Own(runtime::Number{l_num - r_num});
      }

      throw std::runtime_error("incorrect sub operands"s);
    }

    ObjectHolder Mult::Execute(Closure &closure, Context &context) {
      if (!rhs_ || !lhs_) {
        throw std::runtime_error("null operands are not supported"s);
      }
      const auto obj_lhs = lhs_->Execute(closure, context);
      const auto obj_rhs = rhs_->Execute(closure, context);

      const auto ptr_lhs_n = obj_lhs.TryAs<runtime::Number>();
      const auto ptr_rhs_n = obj_rhs.TryAs<runtime::Number>();

      if (ptr_lhs_n != nullptr && ptr_rhs_n != nullptr) {
        const auto &l_num = ptr_lhs_n->GetValue();
        const auto &r_num = ptr_rhs_n->GetValue();

        return ObjectHolder::Own(runtime::Number{l_num * r_num});
      }

      throw std::runtime_error("incorrect mult operands"s);
    }

    ObjectHolder Div::Execute(Closure &closure, Context &context) {
      if (!rhs_ || !lhs_) {
        throw std::runtime_error("null operands are not supported"s);
      }

      const auto obj_lhs = lhs_->Execute(closure, context);
      const auto obj_rhs = rhs_->Execute(closure, context);

      const auto ptr_lhs_n = obj_lhs.TryAs<runtime::Number>();
      const auto ptr_rhs_n = obj_rhs.TryAs<runtime::Number>();

      if (ptr_lhs_n != nullptr && ptr_rhs_n != nullptr) {
        const auto &l_num = ptr_lhs_n->GetValue();
        const auto &r_num = ptr_rhs_n->GetValue();

        if (r_num == 0) {
          throw std::runtime_error("division by zero"s);
        }

        return ObjectHolder::Own(runtime::Number{l_num / r_num});
      }

      throw std::runtime_error("incorrect div operands"s);
    }

    ObjectHolder Or::Execute(Closure &closure, Context &context) {
      if (!rhs_ || !lhs_) {
        throw std::runtime_error("null operands are not supported"s);
      }
      const auto l_obj = lhs_->Execute(closure, context);
      if (runtime::IsTrue(l_obj)) {
        return ObjectHolder::Own(runtime::Bool{true});
      }
      const auto r_obj = rhs_->Execute(closure, context);
      if (runtime::IsTrue(r_obj)) {
        return ObjectHolder::Own(runtime::Bool{true});
      }

      return ObjectHolder::Own(runtime::Bool{false});
    }

    ObjectHolder And::Execute(Closure &closure, Context &context) {
      if (!rhs_ || !lhs_) {
        throw std::runtime_error("null operands are not supported"s);
      }
      const auto &l_obj = lhs_->Execute(closure, context);
      if (!runtime::IsTrue(l_obj)) {
        return ObjectHolder::Own(runtime::Bool{false});
      }
      const auto &r_obj = rhs_->Execute(closure, context);
      if (runtime::IsTrue(l_obj) && runtime::IsTrue(r_obj)) {
        return ObjectHolder::Own(runtime::Bool{true});
      }

      return ObjectHolder::Own(runtime::Bool{false});
    }

    ObjectHolder Not::Execute(Closure &closure, Context &context) {
      if (!argument_) {
        throw std::runtime_error("null operands are not supported"s);
      }
      const auto obj = argument_->Execute(closure, context);
      const auto res = runtime::IsTrue(obj);
      return ObjectHolder::Own(runtime::Bool{!res});
    }

    Comparison::Comparison(Comparator cmp, std::unique_ptr<Statement> lhs, std::unique_ptr<Statement> rhs)
        : BinaryOperation(std::move(lhs), std::move(rhs))
        , cmp_(std::move(cmp)) {
    }

    ObjectHolder Comparison::Execute(Closure &closure, Context &context) {
      if (!rhs_ || !lhs_) {
        throw std::runtime_error("null operands are not supported"s);
      }
      const auto l_obj = lhs_->Execute(closure, context);
      const auto r_obj = rhs_->Execute(closure, context);
      const bool res = cmp_(l_obj, r_obj, context);

      return ObjectHolder::Own(runtime::Bool{res});
    }

    IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
                   std::unique_ptr<Statement> else_body)
        : condition_(std::move(condition))
        , if_body_(std::move(if_body))
        , else_body_(std::move(else_body)) {
    }

    ObjectHolder IfElse::Execute(Closure &closure, Context &context) {
      const auto bool_condition = condition_->Execute(closure, context);

      if (runtime::IsTrue(bool_condition)) {
        return if_body_->Execute(closure, context);
      } else if (else_body_) { // may be empty !!!
        return else_body_->Execute(closure, context);
      } else {
        return {};
      }
    }

  } // namespace ast