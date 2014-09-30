/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation, 
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file Expression.h
 * @date September 18, 2014
 * @author Frank Dellaert
 * @author Paul Furgale
 * @brief Expressions for Block Automatic Differentiation
 */

#pragma once

#include "Expression-inl.h"
#include <boost/bind.hpp>

namespace gtsam {

/**
 * Expression class that supports automatic differentiation
 */
template<typename T>
class Expression {
public:

  // Construct a constant expression
  Expression(const T& value) :
      root_(new ConstantExpression<T>(value)) {
  }

  // Construct a leaf expression
  Expression(const Key& key) :
      root_(new LeafExpression<T>(key)) {
  }

  /// Construct a unary expression
  template<typename E>
  Expression(typename UnaryExpression<T, E>::function f,
      const Expression<E>& expression) {
    // TODO Assert that root of expression is not null.
    root_.reset(new UnaryExpression<T, E>(f, expression));
  }

  /// Construct a binary expression
  template<typename E1, typename E2>
  Expression(typename BinaryExpression<T, E1, E2>::function f,
      const Expression<E1>& expression1, const Expression<E2>& expression2) {
    // TODO Assert that root of expressions 1 and 2 are not null.
    root_.reset(new BinaryExpression<T, E1, E2>(f, expression1, expression2));
  }

  /// Return keys that play in this expression
  std::set<Key> keys() const {
    return root_->keys();
  }

  /// Return value and optional derivatives
  T value(const Values& values,
      boost::optional<std::map<Key, Matrix>&> jacobians = boost::none) const {
    return root_->value(values, jacobians);
  }

  const boost::shared_ptr<ExpressionNode<T> >& root() const {
    return root_;
  }
private:
  boost::shared_ptr<ExpressionNode<T> > root_;
};

// http://stackoverflow.com/questions/16260445/boost-bind-to-operator
template<class T>
struct apply_compose {
  typedef T result_type;
  T operator()(const T& x, const T& y, boost::optional<Matrix&> H1,
      boost::optional<Matrix&> H2) const {
    return x.compose(y, H1, H2);
  }
};

/// Construct a product expression, assumes T::compose(T) -> T
template<typename T>
Expression<T> operator*(const Expression<T>& expression1,
    const Expression<T>& expression2) {
  return Expression<T>(boost::bind(apply_compose<T>(), _1, _2, _3, _4),
      expression1, expression2);
}

}
