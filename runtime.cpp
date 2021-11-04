#include "runtime.h"

#include <algorithm>
#include <cassert>
#include <optional>
#include <string>
#include <utility>

namespace runtime
  {
    using namespace std::literals;
    namespace
      {
        const std::string STR_METHOD = "__str__"s;
        const std::string EQ_METHOD = "__eq__"s;
        const std::string LT_METHOD = "__lt__"s;
      }

    ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
        : data_(std::move(data)) {
    }

    void ObjectHolder::AssertIsValid() const {
      assert(data_ != nullptr);
    }

    ObjectHolder ObjectHolder::Share(Object &object) {
      // Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
      return ObjectHolder(std::shared_ptr<Object>(&object, [](auto * /*p*/) { /* do nothing */ }));
    }

    ObjectHolder ObjectHolder::None() {
      return ObjectHolder();
    }

    Object &ObjectHolder::operator*() const {
      AssertIsValid();
      return *Get();
    }

    Object *ObjectHolder::operator->() const {
      AssertIsValid();
      return Get();
    }

    Object *ObjectHolder::Get() const {
      return data_.get();
    }

    ObjectHolder::operator bool() const {
      return Get() != nullptr;
    }

    bool IsTrue(const ObjectHolder &object) {
      if (const auto *bool_ptr = object.TryAs<Bool>()) {
        return bool_ptr->GetValue();
      }
      if (const auto *number_ptr = object.TryAs<Number>()) {
        return number_ptr->GetValue();
      }
      if (const auto *string_ptr = object.TryAs<String>()) {
        return !string_ptr->GetValue().empty();
      }
      return false;

    }

    void ClassInstance::Print(std::ostream &os, Context &context) {
      if (HasMethod(STR_METHOD, 0)) {
        const auto obj = Call(STR_METHOD, {}, context);
        if (const auto obj_ptr = obj.Get()) {
          obj_ptr->Print(os, context);
        }
      } else {
        os << this;
      }
    }

    bool ClassInstance::HasMethod(const std::string &method, size_t argument_count) const {
      const auto *method_ptr = cls_.GetMethod(method);
      return method_ptr != nullptr && method_ptr->formal_params.size() == argument_count;
    }

    Closure &ClassInstance::Fields() {
      return closure_;
    }

    const Closure &ClassInstance::Fields() const {
      return closure_;
    }

    ClassInstance::ClassInstance(const Class &cls)
        : cls_(cls) {
    }

    ObjectHolder ClassInstance::Call(const std::string &method,
                                     const std::vector<ObjectHolder> &actual_args,
                                     Context &context) {
      if (HasMethod(method, actual_args.size())) {
        Closure closure;
        const auto method_ptr = cls_.GetMethod(method);
        for (size_t i = 0; i < actual_args.size(); ++i) {
          closure.emplace(method_ptr->formal_params.at(i), actual_args[i]);
        }
        closure.emplace("self"s, ObjectHolder::Share(*this));
        auto* const body_ptr = method_ptr->body.get();
        const auto result = body_ptr->Execute(closure, context);
        return result;
      } else {
        throw std::runtime_error("Nothing to call"s);
      }
    }

    Class::Class(std::string name, std::vector<Method> methods, const Class *parent)
        : name_(std::move(name))
        , methods_(std::move(methods))
        , parent_(parent) {
    }

    const Method *Class::GetMethod(const std::string &name) const {
      for (const auto &method: methods_) {
        if (method.name == name) {
          return &method;
        }
      }
      if (parent_ != nullptr) {
        return parent_->GetMethod(name);
      } else {
        return nullptr;
      }
    }

    [[nodiscard]] const std::string &Class::GetName() const {
      return name_;
    }

    void Class::Print(std::ostream &os, [[maybe_unused]] Context &context) {
      os << "Class "s << GetName();
    }

    void Bool::Print(std::ostream &os, [[maybe_unused]] Context &context) {
      os << (GetValue() ? "True"sv : "False"sv);
    }

    bool Equal(const ObjectHolder &lhs, const ObjectHolder &rhs, Context &context) {
      if (!lhs && !rhs) {
        return true;
      }
      if (lhs.TryAs<Number>() && rhs.TryAs<Number>()) {
        return lhs.TryAs<Number>()->GetValue() == rhs.TryAs<Number>()->GetValue();
      }
      if (lhs.TryAs<String>() && rhs.TryAs<String>()) {
        const auto &lhs_str = lhs.TryAs<String>()->GetValue();
        const auto &rhs_str = rhs.TryAs<String>()->GetValue();
        return lhs_str == rhs_str;
      }
      if (lhs.TryAs<Bool>() && rhs.TryAs<Bool>()) {
        return lhs.TryAs<Bool>()->GetValue() == rhs.TryAs<Bool>()->GetValue();
      }
      auto *lhs_ptr = lhs.TryAs<ClassInstance>();
      if (lhs_ptr != nullptr && lhs_ptr->HasMethod(EQ_METHOD, 1)) {
        return IsTrue(lhs_ptr->Call(EQ_METHOD, {rhs}, context));
      }
      throw std::runtime_error("Cannot compare objects"s);
    }

    bool Less(const ObjectHolder &lhs, const ObjectHolder &rhs, Context &context) {
      if (!lhs && !rhs) {
        throw std::runtime_error("Cannot compare objects for less"s);
      }
      if (lhs.TryAs<Number>() && rhs.TryAs<Number>()) {
        return lhs.TryAs<Number>()->GetValue() < rhs.TryAs<Number>()->GetValue();
      }
      if (lhs.TryAs<String>() && rhs.TryAs<String>()) {
        const auto &lhs_str = lhs.TryAs<String>()->GetValue();
        const auto &rhs_str = rhs.TryAs<String>()->GetValue();
        return lhs_str < rhs_str;
      }
      if (lhs.TryAs<Bool>() && rhs.TryAs<Bool>()) {
        return lhs.TryAs<Bool>()->GetValue() < rhs.TryAs<Bool>()->GetValue();
      }
      auto *lhs_ptr = lhs.TryAs<ClassInstance>();
      if (lhs_ptr != nullptr && lhs_ptr->HasMethod(LT_METHOD, 1)) {
        return IsTrue(lhs_ptr->Call(LT_METHOD, {rhs}, context));
      }
      throw std::runtime_error("Cannot compare objects for less"s);
    }

    bool NotEqual(const ObjectHolder &lhs, const ObjectHolder &rhs, Context &context) {
      return !Equal(lhs, rhs, context);
    }

    bool Greater(const ObjectHolder &lhs, const ObjectHolder &rhs, Context &context) {
      return !LessOrEqual(lhs, rhs, context);
    }

    bool LessOrEqual(const ObjectHolder &lhs, const ObjectHolder &rhs, Context &context) {
      return Less(lhs, rhs, context) || Equal(lhs, rhs, context);
    }

    bool GreaterOrEqual(const ObjectHolder &lhs, const ObjectHolder &rhs, Context &context) {
      return !Less(lhs, rhs, context);
    }

  }  // namespace runtime