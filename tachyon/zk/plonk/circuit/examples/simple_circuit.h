// Copyright 2020-2022 The Electric Coin Company
// Copyright 2022 The Halo2 developers
// Use of this source code is governed by a MIT/Apache-2.0 style license that
// can be found in the LICENSE-MIT.halo2 and the LICENCE-APACHE.halo2
// file.

#ifndef TACHYON_ZK_PLONK_CIRCUIT_EXAMPLES_SIMPLE_CIRCUIT_H_
#define TACHYON_ZK_PLONK_CIRCUIT_EXAMPLES_SIMPLE_CIRCUIT_H_

#include <array>
#include <memory>
#include <utility>
#include <vector>

#include "tachyon/zk/base/value.h"
#include "tachyon/zk/plonk/circuit/circuit.h"
#include "tachyon/zk/plonk/constraint_system.h"

namespace tachyon::zk {

template <typename F>
class SimpleCircuit;

// Chip state is stored in a config struct. This is generated by the chip
// during configuration, and then stored inside the chip.
template <typename F>
class FieldConfig {
 public:
  using Field = F;

  FieldConfig() = default;

  const std::array<AdviceColumnKey, 2>& advice() const { return advice_; }
  const InstanceColumnKey& instance() const { return instance_; }
  const Selector& s_mul() const { return s_mul_; }

 private:
  FieldConfig(std::array<AdviceColumnKey, 2> advice, InstanceColumnKey instance,
              Selector s_mul)
      : advice_(std::move(advice)),
        instance_(std::move(instance)),
        s_mul_(std::move(s_mul)) {}

  // For this chip, we will use two advice columns to implement our
  // instructions. These are also the columns through which we communicate with
  // other parts of the circuit.
  std::array<AdviceColumnKey, 2> advice_;
  // This is the public input (instance) column.
  InstanceColumnKey instance_;
  // We need a selector to enable the multiplication gate, so that we aren't
  // placing any constraints on cells where |NumericInstructions::Mul| is not
  // being used. This is important when building larger circuits, where columns
  // are used by multiple sets of instructions.
  Selector s_mul_;
};

template <typename F>
class FieldChip {
 public:
  FieldChip() = default;

  static FieldConfig<F> Configure(ConstraintSystem<F>& meta,
                                  std::array<AdviceColumnKey, 2> advice,
                                  InstanceColumnKey instance,
                                  FixedColumnKey constant) {
    meta.EnableEquality(instance);
    meta.EnableConstant(constant);
    for (const AdviceColumnKey& column : advice) {
      meta.EnableEquality(column);
    }
    Selector sel = meta.CreateSimpleSelector();

    // Define our multiplication gate!
    meta.CreateGate("mul", [&meta, &advice, &sel]() {
      // To implement multiplication, we need three advice cells and a selector
      // cell. We arrange them like so:
      //
      // | a0  | a1  | sel   |
      // |-----|-----|-------|
      // | lhs | rhs | s_mul |
      // | out |     |       |
      //
      // Gates may refer to any relative offsets we want, but each distinct
      // offset adds a cost to the proof. The most common offsets are 0 (the
      // current row), 1 (the next row), and -1 (the previous row), for which
      // |Rotation| has specific constructors.
      std::unique_ptr<Expression<F>> lhs =
          meta.QueryAdvice(advice[0], Rotation::Cur());
      std::unique_ptr<Expression<F>> rhs =
          meta.QueryAdvice(advice[1], Rotation::Cur());
      std::unique_ptr<Expression<F>> out =
          meta.QueryAdvice(advice[0], Rotation::Next());
      std::unique_ptr<Expression<F>> s_mul = meta.QuerySelector(sel);

      // Finally, we return the polynomial expressions that constrain this gate.
      // For our multiplication gate, we only need a single polynomial
      // constraint.
      //
      // The polynomial expressions returned from |CreateGate()| will be
      // constrained by the proving system to equal zero. Our expression
      // has the following properties:
      // - When s_mul = 0, any value is allowed in lhs, rhs, and out.
      // - When s_mul != 0, this constrains lhs * rhs = out.
      return std::vector<Constraint<F>>(
          {std::move(s_mul) *
           (std::move(lhs) * std::move(rhs) - std::move(out))});
    });

    return FieldConfig<F>(std::move(advice), std::move(instance),
                          std::move(sel));
  }

  Error LoadPrivate(Layouter<F>* layouter, const Value<F>& value,
                    AssignedCell<F>* cell) const {
    return layouter->AssignRegion("load private",
                                  [this, cell, &value](Region<F>& region) {
                                    return region.AssignAdvice(
                                        "private input", config_.advice()[0], 0,
                                        [&value]() { return value; }, cell);
                                  });
  }

  Error LoadConstant(Layouter<F>* layouter, const F& constant,
                     AssignedCell<F>* cell) const {
    return layouter->AssignRegion(
        "load constant", [this, &constant, cell](Region<F>& region) {
          return region.AssignAdviceFromConstant(
              "constant value", config_.advice()[0], 0, constant, cell);
        });
  }

  Error Mul(Layouter<F>* layouter, const AssignedCell<F>& a,
            const AssignedCell<F>& b, AssignedCell<F>* cell) const {
    return layouter->AssignRegion("mul", [this, a, b, cell](Region<F>& region) {
      // We only want to use a single multiplication gate in this |region|,
      // so we enable it at |region| offset 0; this means it will constrain
      // cells at offsets 0 and 1.
      Error error = config_.s_mul().Enable(region, 0);
      if (error != Error::kNone) return error;

      // The inputs we've been given could be located anywhere in the
      // circuit, but we can only rely on relative offsets inside this
      // region. So we assign new cells inside the |region| and constrain
      // them to have the same values as the inputs.
      AssignedCell<F> new_a;
      error = a.CopyAdvice("lhs", region, config_.advice()[0], 0, &new_a);
      if (error != Error::kNone) return error;
      AssignedCell<F> new_b;
      error = b.CopyAdvice("rhs", region, config_.advice()[1], 0, &new_b);
      if (error != Error::kNone) return error;

      // Now we can assign the multiplication result, which is to be
      // assigned into the output position.
      Value<F> value = a.value() * b.value();

      // Finally, we do the assignment to the output, returning a
      // variable to be used in another part of the circuit.
      return region.AssignAdvice(
          "lhs * rhs", config_.advice()[0], 1, [&value]() { return value; },
          cell);
    });
  }

  Error ExposePublic(Layouter<F>* layouter, const AssignedCell<F>& cell,
                     size_t row) const {
    return layouter->ConstrainInstance(cell.cell(), config_.instance(), row);
  }

 private:
  friend class SimpleCircuit<F>;

  explicit FieldChip(FieldConfig<F> config) : config_(std::move(config)) {}

  FieldConfig<F> config_;
};

template <typename F>
class SimpleCircuit : public Circuit<FieldConfig<F>> {
 public:
  SimpleCircuit() = default;

  std::unique_ptr<Circuit<FieldConfig<F>>> WithoutWitness() const override {
    return std::make_unique<SimpleCircuit>();
  }

  static FieldConfig<F> Configure(ConstraintSystem<F>& meta) {
    // We create the two advice columns that FieldChip uses for I/O.
    std::array<AdviceColumnKey, 2> advice(
        {meta.CreateAdviceColumn(), meta.CreateAdviceColumn()});

    // We also need an instance column to store public inputs.
    InstanceColumnKey instance = meta.CreateInstanceColumn();

    // Create a fixed column to load constants.
    FixedColumnKey constant = meta.CreateFixedColumn();

    return FieldChip<F>::Configure(meta, std::move(advice), std::move(instance),
                                   std::move(constant));
  }

  Error Synthesize(FieldConfig<F> config, Layouter<F>* layouter) override {
    FieldChip<F> field_chip(std::move(config));

    // Load our private values into the circuit.
    AssignedCell<F> a;
    Error error =
        field_chip.LoadPrivate(layouter->Namespace("load a").get(), a_, &a);
    if (error != Error::kNone) return error;
    AssignedCell<F> b;
    error = field_chip.LoadPrivate(layouter->Namespace("load b").get(), b_, &b);
    if (error != Error::kNone) return error;

    // Load the constant factor into the circuit.
    AssignedCell<F> constant;
    error = field_chip.LoadConstant(layouter->Namespace("load constant").get(),
                                    constant_, &constant);
    if (error != Error::kNone) return error;

    // We only have access to plain multiplication.
    // We could implement our circuit as:
    //     asq  = a²
    //     bsq  = b²
    //     absq = asq * bsq
    //     c    = constant * asq * bsq
    //
    // but it's more efficient to implement it as:
    //     ab   = a * b
    //     absq = ab²
    //     c    = constant * absq
    AssignedCell<F> ab;
    error = field_chip.Mul(layouter->Namespace("a * b").get(), a, b, &ab);
    if (error != Error::kNone) return error;
    AssignedCell<F> absq;
    error = field_chip.Mul(layouter->Namespace("ab * ab").get(), ab, ab, &absq);
    if (error != Error::kNone) return error;
    AssignedCell<F> c;
    error = field_chip.Mul(layouter->Namespace("constant * absq").get(),
                           constant, absq, &c);
    if (error != Error::kNone) return error;

    // Expose the result as a public input to the circuit.
    return field_chip.ExposePublic(layouter->Namespace("expose c").get(), c, 0);
  }

 private:
  F constant_;
  Value<F> a_;
  Value<F> b_;
};

}  // namespace tachyon::zk

#endif  // TACHYON_ZK_PLONK_CIRCUIT_EXAMPLES_SIMPLE_CIRCUIT_H_
