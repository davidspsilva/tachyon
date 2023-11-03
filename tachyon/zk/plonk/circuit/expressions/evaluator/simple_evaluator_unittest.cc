#include "tachyon/zk/plonk/circuit/expressions/evaluator/simple_evaluator.h"

#include <memory>

#include "tachyon/zk/plonk/circuit/expressions/evaluator/test/evaluator_test.h"
#include "tachyon/zk/plonk/circuit/expressions/expression_factory.h"

namespace tachyon::zk {

namespace {

using Expr = std::unique_ptr<Expression<GF7>>;

class SimpleEvaluatorTest : public EvaluatorTest {
 public:
  SimpleEvaluatorTest() {
    SimpleEvaluator<Poly>::Arguments arguments(&fixed_polys_, &advice_polys_,
                                               &instance_polys_, &challenges_);
    simple_evaluator_ =
        std::make_unique<SimpleEvaluator<Poly>>(3, 4, 1, arguments);
  }
  SimpleEvaluatorTest(const SimpleEvaluatorTest&) = delete;
  SimpleEvaluatorTest& operator=(const SimpleEvaluatorTest&) = delete;
  ~SimpleEvaluatorTest() override = default;

 protected:
  std::unique_ptr<SimpleEvaluator<Poly>> simple_evaluator_;
};

}  // namespace

TEST_F(SimpleEvaluatorTest, Constant) {
  GF7 value = GF7::Random();
  Expr expr = ExpressionFactory<GF7>::Constant(value);
  int32_t last_idx = simple_evaluator_->idx();
  GF7 evaluated = simple_evaluator_->Evaluate(expr.get());
  EXPECT_EQ(value, evaluated);
  EXPECT_EQ(simple_evaluator_->idx(), last_idx + 1);
}

TEST_F(SimpleEvaluatorTest, Selector) {
  Expr expr = ExpressionFactory<GF7>::Selector(Selector::Simple(1));
  EXPECT_DEATH(simple_evaluator_->Evaluate(expr.get()), "");
}

TEST_F(SimpleEvaluatorTest, Fixed) {
  struct {
    int32_t rotation;
    size_t column_index;
  } tests[] = {
      // fixed_polys_[0], (3 + 1 * 1) % 4 = 0, coefficient[0]
      {1, 0},
      // fixed_polys_[1], (4 + 2 * 1) % 4 = 2, coefficient[2]
      {2, 1},
  };

  for (const auto& test : tests) {
    int32_t idx = simple_evaluator_->idx();
    int32_t rot_scale = simple_evaluator_->rot_scale();
    int32_t size = simple_evaluator_->size();
    FixedQuery query(1, Rotation(test.rotation),
                     FixedColumn(test.column_index));
    size_t coeff_index = query.rotation().GetIndex(idx, rot_scale, size);

    const GF7* expected = fixed_polys_[test.column_index][coeff_index];
    Expr expr = ExpressionFactory<GF7>::Fixed(query);
    GF7 evaluated = simple_evaluator_->Evaluate(expr.get());
    EXPECT_EQ(evaluated, *expected);
  }
}

TEST_F(SimpleEvaluatorTest, Advice) {
  struct {
    int32_t rotation;
    size_t column_index;
  } tests[] = {
      // advice_polys_[2], (3 + 6 * 1) % 4 = 1, coefficient[1]
      {6, 2},
      // advice_polys_[3], (4 + 7 * 1) % 4 = 3, coefficient[3]
      {7, 3},
  };

  for (const auto& test : tests) {
    int32_t idx = simple_evaluator_->idx();
    int32_t rot_scale = simple_evaluator_->rot_scale();
    int32_t size = simple_evaluator_->size();
    AdviceQuery query(1, Rotation(test.rotation),
                      AdviceColumn(test.column_index, Phase(0)));
    size_t coeff_index = query.rotation().GetIndex(idx, rot_scale, size);

    const GF7* expected = advice_polys_[test.column_index][coeff_index];
    Expr expr = ExpressionFactory<GF7>::Advice(query);
    GF7 evaluated = simple_evaluator_->Evaluate(expr.get());
    EXPECT_EQ(evaluated, *expected);
  }
}

TEST_F(SimpleEvaluatorTest, Instance) {
  struct {
    int32_t rotation;
    size_t column_index;
  } tests[] = {
      // instance_polys_[1], (3 + 1 * 1) % 4 = 0, coefficient[0]
      {1, 1},
      // instance_polys_[2], (4 + 2 * 1) % 4 = 2, coefficient[2]
      {2, 2},
  };

  for (const auto& test : tests) {
    int32_t idx = simple_evaluator_->idx();
    int32_t rot_scale = simple_evaluator_->rot_scale();
    int32_t size = simple_evaluator_->size();
    InstanceQuery query(1, Rotation(test.rotation),
                        InstanceColumn(test.column_index));
    size_t coeff_index = query.rotation().GetIndex(idx, rot_scale, size);

    const GF7* expected = instance_polys_[test.column_index][coeff_index];
    Expr expr = ExpressionFactory<GF7>::Instance(query);
    GF7 evaluated = simple_evaluator_->Evaluate(expr.get());
    EXPECT_EQ(evaluated, *expected);
  }
}

TEST_F(SimpleEvaluatorTest, Challenges) {
  for (size_t i = 0; i < challenges_.size(); ++i) {
    Expr expr = ExpressionFactory<GF7>::Challenge(Challenge(i, Phase(0)));
    GF7 evaluated = simple_evaluator_->Evaluate(expr.get());
    GF7 expected = challenges_[i];
    EXPECT_EQ(evaluated, expected);
  }
}

TEST_F(SimpleEvaluatorTest, Negated) {
  GF7 value = GF7::Random();
  Expr expr =
      ExpressionFactory<GF7>::Negated(ExpressionFactory<GF7>::Constant(value));
  GF7 negated = simple_evaluator_->Evaluate(expr.get());
  EXPECT_EQ(negated, -value);
}

TEST_F(SimpleEvaluatorTest, Sum) {
  GF7 a = GF7::Random();
  GF7 b = GF7::Random();
  Expr expr = ExpressionFactory<GF7>::Sum(ExpressionFactory<GF7>::Constant(a),
                                          ExpressionFactory<GF7>::Constant(b));
  GF7 evaluated = simple_evaluator_->Evaluate(expr.get());
  EXPECT_EQ(a + b, evaluated);
}

TEST_F(SimpleEvaluatorTest, Product) {
  GF7 a = GF7::Random();
  GF7 b = GF7::Random();
  Expr expr = ExpressionFactory<GF7>::Product(
      ExpressionFactory<GF7>::Constant(a), ExpressionFactory<GF7>::Constant(b));
  GF7 evaluated = simple_evaluator_->Evaluate(expr.get());
  EXPECT_EQ(a * b, evaluated);
}

TEST_F(SimpleEvaluatorTest, Scaled) {
  GF7 a = GF7::Random();
  GF7 b = GF7::Random();
  Expr expr =
      ExpressionFactory<GF7>::Scaled(ExpressionFactory<GF7>::Constant(a), b);
  GF7 scaled_expr = simple_evaluator_->Evaluate(expr.get());
  EXPECT_EQ(scaled_expr, a * b);
}

}  // namespace tachyon::zk