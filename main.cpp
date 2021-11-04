#include "lexer.h"
#include "parse.h"
#include "runtime.h"
#include "statement.h"
#include "test_runner_p.h"

#include <iostream>

using namespace std;

namespace parse
  {
    void RunOpenLexerTests(TestRunner &tr);
  }  // namespace parse

namespace ast
  {
    void RunUnitTests(TestRunner &tr);
  }
namespace runtime
  {
    void RunObjectHolderTests(TestRunner &tr);
    void RunObjectsTests(TestRunner &tr);
  }  // namespace runtime

void TestParseProgram(TestRunner &tr);

namespace
  {

    void RunMythonProgram(istream &input, ostream &output) {
      parse::Lexer lexer(input);
      auto program = ParseProgram(lexer);

      runtime::SimpleContext context{output};
      runtime::Closure closure;
      program->Execute(closure, context);
    }

    void TestSimplePrints() {
      istringstream input(R"(
print 57
print 10, 24, -8
print 'hello'
print "world"
print True, False
print
print None
)");

      ostringstream output;
      RunMythonProgram(input, output);

      ASSERT_EQUAL(output.str(), "57\n10 24 -8\nhello\nworld\nTrue False\n\nNone\n");
    }

    void TestAssignments() {
      istringstream input(R"(
x = 57
print x
x = 'C++ black belt'
print x
y = False
x = y
print x
x = None
print x, y
)");

      ostringstream output;
      RunMythonProgram(input, output);

      ASSERT_EQUAL(output.str(), "57\nC++ black belt\nFalse\nNone False\n");
    }

    void TestArithmetics() {
      istringstream input("print 1+2+3+4+5, 1*2*3*4*5, 1-2-3-4-5, 36/4/3, 2*5+10/2");

      ostringstream output;
      RunMythonProgram(input, output);

      ASSERT_EQUAL(output.str(), "15 120 -13 3 15\n");
    }

    void TestVariablesArePointers() {
      istringstream input(R"(
class Counter:
  def __init__():
    self.value = 0

  def add():
    self.value = self.value + 1

class Dummy:
  def do_add(counter):
    counter.add()

x = Counter()
y = x

x.add()
y.add()

print x.value

d = Dummy()
d.do_add(x)

print y.value
)");

      ostringstream output;
      RunMythonProgram(input, output);

      ASSERT_EQUAL(output.str(), "2\n3\n");
    }

    void TestShortCircuitEvaluation() {
      istringstream input(R"(
class Z:
  def f():
    print "Should not be executed"
    return True

z = Z()
x = True or z.f()
x = False and z.f()
)");

      ostringstream output;
      RunMythonProgram(input, output);

      ASSERT_EQUAL(output.str(), "");
    }

    void TestSegmentationFault() {
      istringstream input(R"(
a = 123
a.b = 456
)");

      ostringstream output;
      ASSERT_THROWS(RunMythonProgram(input, output), std::runtime_error);

    }

    void TestPrint() {
      istringstream input(R"(
a = 123
print a.b
)");

      ostringstream output;
      ASSERT_THROWS(RunMythonProgram(input, output), std::runtime_error);
    }

    void TestNonClassMethodCall() {
      istringstream input(R"(
x = 123
x.f()
)");

      ostringstream output;
      RunMythonProgram(input, output);
    }

    void TestMethodOverloading() {
      istringstream input1(R"(
class X:
  def f(a):
    print "one parameter overload"

  def f(a, b):
    print "two parameters overload"

x = X()
x.f(1)
)");

      istringstream input2(R"(
class X:
  def f(a):
    print "one parameter overload"

  def f(a, b):
    print "two parameters overload"

x = X()
x.f(1, 2)
)");

      ostringstream output;
      bool e1 = false;
      try {
        RunMythonProgram(input1, output);
      } catch (const std::runtime_error &) {
        e1 = true;
      }
      bool e2 = false;
      try {
        RunMythonProgram(input2, output);
      } catch (const std::runtime_error &) {
        e2 = true;
      }
      ASSERT(e1 == e2);
    }

    void TestNonClassFieldAssignment() {
      istringstream input(R"(
n = 123
n.x = 456
)");

      ostringstream output;
      ASSERT_THROWS(RunMythonProgram(input, output), std::runtime_error);
    }

    void TestWithParameter() {
      istringstream input(R"(
class X:
  def __str__():
    return "X"

class Sink:
  def apply(a):
    pass = 0

sink = Sink()

n = 123
sink.apply(X())
print n
)");

      ostringstream output;
      RunMythonProgram(input, output);
      ASSERT(output.str() == "123\n");
    }

    void Test() {
      istringstream input(R"(
class X:
  def __str__():
    return "X"

class Sink:
  def apply():
    pass = 0

sink = Sink()

n = 123
sink.apply(X())
print n
)");

      ostringstream output;
      RunMythonProgram(input, output);
      ASSERT(output.str() == "123\n");
    }

    void TestAll() {
      TestRunner tr;
      parse::RunOpenLexerTests(tr);
      runtime::RunObjectHolderTests(tr);
      runtime::RunObjectsTests(tr);
      ast::RunUnitTests(tr);
      TestParseProgram(tr);

      RUN_TEST(tr, TestSimplePrints);
      RUN_TEST(tr, TestAssignments);
      RUN_TEST(tr, TestArithmetics);
      RUN_TEST(tr, TestVariablesArePointers);
      RUN_TEST(tr, TestShortCircuitEvaluation);
      RUN_TEST(tr, TestSegmentationFault);
      RUN_TEST(tr, TestPrint);
      RUN_TEST(tr, TestNonClassMethodCall);
      RUN_TEST(tr, TestMethodOverloading);
      RUN_TEST(tr, TestNonClassFieldAssignment);
      RUN_TEST(tr, Test);
      RUN_TEST(tr, TestWithParameter);
    }

  }  // namespace

int main() {
  try {
    TestAll();

    RunMythonProgram(cin, cout);
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  return 0;
}