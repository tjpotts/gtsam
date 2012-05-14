/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file    ISAM2.h
 * @brief   Incremental update functionality (ISAM2) for BayesTree, with fluid relinearization.
 * @author  Michael Kaess, Richard Roberts
 */

// \callgraph

#pragma once

#include <gtsam/nonlinear/NonlinearFactorGraph.h>
#include <gtsam/nonlinear/DoglegOptimizerImpl.h>
#include <gtsam/linear/GaussianBayesTree.h>

#include <boost/variant.hpp>

namespace gtsam {

/**
 * @defgroup ISAM2
 */

/**
 * @ingroup ISAM2
 * Parameters for ISAM2 using Gauss-Newton optimization.  Either this class or
 * ISAM2DoglegParams should be specified as the optimizationParams in
 * ISAM2Params, which should in turn be passed to ISAM2(const ISAM2Params&).
 */
struct ISAM2GaussNewtonParams {
  double wildfireThreshold; ///< Continue updating the linear delta only when changes are above this threshold (default: 0.001)

  /** Specify parameters as constructor arguments */
  ISAM2GaussNewtonParams(
      double _wildfireThreshold = 0.001 ///< see ISAM2GaussNewtonParams public variables, ISAM2GaussNewtonParams::wildfireThreshold
  ) : wildfireThreshold(_wildfireThreshold) {}
};

/**
 * @ingroup ISAM2
 * Parameters for ISAM2 using Dogleg optimization.  Either this class or
 * ISAM2GaussNewtonParams should be specified as the optimizationParams in
 * ISAM2Params, which should in turn be passed to ISAM2(const ISAM2Params&).
 */
struct ISAM2DoglegParams {
  double initialDelta; ///< The initial trust region radius for Dogleg
  double wildfireThreshold; ///< Continue updating the linear delta only when changes are above this threshold (default: 0.001)
  DoglegOptimizerImpl::TrustRegionAdaptationMode adaptationMode; ///< See description in DoglegOptimizerImpl::TrustRegionAdaptationMode
  bool verbose; ///< Whether Dogleg prints iteration and convergence information

  /** Specify parameters as constructor arguments */
  ISAM2DoglegParams(
      double _initialDelta = 1.0, ///< see ISAM2DoglegParams::initialDelta
      double _wildfireThreshold = 1e-5, ///< see ISAM2DoglegParams::wildfireThreshold
      DoglegOptimizerImpl::TrustRegionAdaptationMode _adaptationMode = DoglegOptimizerImpl::SEARCH_EACH_ITERATION, ///< see ISAM2DoglegParams::adaptationMode
      bool _verbose = false ///< see ISAM2DoglegParams::verbose
  ) : initialDelta(_initialDelta), wildfireThreshold(_wildfireThreshold),
  adaptationMode(_adaptationMode), verbose(_verbose) {}
};

/**
 * @ingroup ISAM2
 * Parameters for the ISAM2 algorithm.  Default parameter values are listed below.
 */
struct ISAM2Params {
  typedef boost::variant<ISAM2GaussNewtonParams, ISAM2DoglegParams> OptimizationParams; ///< Either ISAM2GaussNewtonParams or ISAM2DoglegParams
  typedef boost::variant<double, FastMap<char,Vector> > RelinearizationThreshold; ///< Either a constant relinearization threshold or a per-variable-type set of thresholds

  /** Optimization parameters, this both selects the nonlinear optimization
   * method and specifies its parameters, either ISAM2GaussNewtonParams or
   * ISAM2DoglegParams.  In the former, Gauss-Newton optimization will be used
   * with the specified parameters, and in the latter Powell's dog-leg
   * algorithm will be used with the specified parameters.
   */
  OptimizationParams optimizationParams;

  /** Only relinearize variables whose linear delta magnitude is greater than
   * this threshold (default: 0.1).  If this is a FastMap<char,Vector> instead
   * of a double, then the threshold is specified for each dimension of each
   * variable type.  This parameter then maps from a character indicating the
   * variable type to a Vector of thresholds for each dimension of that
   * variable.  For example, if Pose keys are of type TypedSymbol<'x',Pose3>,
   * and landmark keys are of type TypedSymbol<'l',Point3>, then appropriate
   * entries would be added with:
   * \code
     FastMap<char,Vector> thresholds;
     thresholds[PoseKey::chr()] = Vector_(6, 0.1, 0.1, 0.1, 0.5, 0.5, 0.5); // 0.1 rad rotation threshold, 0.5 m translation threshold
     thresholds[PointKey::chr()] = Vector_(3, 1.0, 1.0, 1.0);               // 1.0 m landmark position threshold
     params.relinearizeThreshold = thresholds;
     \endcode
   */
  RelinearizationThreshold relinearizeThreshold;

  int relinearizeSkip; ///< Only relinearize any variables every relinearizeSkip calls to ISAM2::update (default: 10)

  bool enableRelinearization; ///< Controls whether ISAM2 will ever relinearize any variables (default: true)

  bool evaluateNonlinearError; ///< Whether to evaluate the nonlinear error before and after the update, to return in ISAM2Result from update()

  enum Factorization { LDL, QR };
  /** Specifies whether to use QR or LDL numerical factorization (default: LDL).
   * LDL is faster but potentially numerically unstable for poorly-conditioned problems, which can occur when
   * uncertainty is very low in some variables (or dimensions of variables) and very high in others.  QR is
   * slower but more numerically stable in poorly-conditioned problems.  We suggest using the default of LDL
   * unless gtsam sometimes throws NegativeMatrixException when your problem's Hessian is actually positive
   * definite.  For positive definite problems, numerical error accumulation can cause the problem to become
   * numerically negative or indefinite as solving proceeds, especially when using LDL.
   */
  Factorization factorization;

  /** Whether to cache linear factors (default: true).
   * This can improve performance if linearization is expensive, but can hurt
   * performance if linearization is very cleap due to computation to look up
   * additional keys.
   */
  bool cacheLinearizedFactors;

  KeyFormatter keyFormatter; ///< A KeyFormatter for when keys are printed during debugging (default: DefaultKeyFormatter)

  bool enableDetailedResults; ///< Whether to compute and return ISAM2Result::detailedResults, this can increase running time (default: false)

  /** Specify parameters as constructor arguments */
  ISAM2Params(
      OptimizationParams _optimizationParams = ISAM2GaussNewtonParams(), ///< see ISAM2Params::optimizationParams
      RelinearizationThreshold _relinearizeThreshold = 0.1, ///< see ISAM2Params::relinearizeThreshold
      int _relinearizeSkip = 10, ///< see ISAM2Params::relinearizeSkip
      bool _enableRelinearization = true, ///< see ISAM2Params::enableRelinearization
      bool _evaluateNonlinearError = false, ///< see ISAM2Params::evaluateNonlinearError
      Factorization _factorization = ISAM2Params::LDL, ///< see ISAM2Params::factorization
      bool _cacheLinearizedFactors = true, ///< see ISAM2Params::cacheLinearizedFactors
      const KeyFormatter& _keyFormatter = DefaultKeyFormatter ///< see ISAM2::Params::keyFormatter
  ) : optimizationParams(_optimizationParams), relinearizeThreshold(_relinearizeThreshold),
      relinearizeSkip(_relinearizeSkip), enableRelinearization(_enableRelinearization),
      evaluateNonlinearError(_evaluateNonlinearError), factorization(_factorization),
      cacheLinearizedFactors(_cacheLinearizedFactors), keyFormatter(_keyFormatter),
      enableDetailedResults(false) {}
};

/**
 * @ingroup ISAM2
 * This struct is returned from ISAM2::update() and contains information about
 * the update that is useful for determining whether the solution is
 * converging, and about how much work was required for the update.  See member
 * variables for details and information about each entry.
 */
struct ISAM2Result {
  /** The nonlinear error of all of the factors, \a including new factors and
   * variables added during the current call to ISAM2::update().  This error is
   * calculated using the following variable values:
   * \li Pre-existing variables will be evaluated by combining their
   * linearization point before this call to update, with their partial linear
   * delta, as computed by ISAM2::calculateEstimate().
   * \li New variables will be evaluated at their initialization points passed
   * into the current call to update.
   * \par Note: This will only be computed if ISAM2Params::evaluateNonlinearError
   * is set to \c true, because there is some cost to this computation.
   */
  boost::optional<double> errorBefore;

  /** The nonlinear error of all of the factors computed after the current
   * update, meaning that variables above the relinearization threshold
   * (ISAM2Params::relinearizeThreshold) have been relinearized and new
   * variables have undergone one linear update.  Variable values are
   * again computed by combining their linearization points with their
   * partial linear deltas, by ISAM2::calculateEstimate().
   * \par Note: This will only be computed if ISAM2Params::evaluateNonlinearError
   * is set to \c true, because there is some cost to this computation.
   */
  boost::optional<double> errorAfter;

  /** The number of variables that were relinearized because their linear
   * deltas exceeded the reslinearization threshold
   * (ISAM2Params::relinearizeThreshold), combined with any additional
   * variables that had to be relinearized because they were involved in
   * the same factor as a variable above the relinearization threshold.
   * On steps where no relinearization is considered
   * (see ISAM2Params::relinearizeSkip), this count will be zero.
   */
  size_t variablesRelinearized;

  /** The number of variables that were reeliminated as parts of the Bayes'
   * Tree were recalculated, due to new factors.  When loop closures occur,
   * this count will be large as the new loop-closing factors will tend to
   * involve variables far away from the root, and everything up to the root
   * will be reeliminated.
   */
  size_t variablesReeliminated;

  /** The number of cliques in the Bayes' Tree */
  size_t cliques;

  /** The indices of the newly-added factors, in 1-to-1 correspondence with the
   * factors passed as \c newFactors to ISAM2::update().  These indices may be
   * used later to refer to the factors in order to remove them.
   */
  FastVector<size_t> newFactorsIndices;

  /** A struct holding detailed results, which must be enabled with
   * ISAM2Params::enableDetailedResults.
   */
  struct DetailedResults {
    /** The status of a single variable, this struct is stored in
     * DetailedResults::variableStatus */
    struct VariableStatus {
      /** Whether the variable was just reeliminated, due to being relinearized,
       * observed, new, or on the path up to the root clique from another
       * reeliminated variable. */
      bool isReeliminated;
      bool isAboveRelinThreshold; ///< Whether the variable was just relinearized due to being above the relinearization threshold
      bool isRelinearizeInvolved; ///< Whether the variable was below the relinearization threshold but was relinearized by being involved in a factor with a variable above the relinearization threshold
      bool isRelinearized; /// Whether the variable was relinearized, either by being above the relinearization threshold or by involvement.
      bool isObserved; ///< Whether the variable was just involved in new factors
      bool isNew; ///< Whether the variable itself was just added
      bool inRootClique; ///< Whether the variable is in the root clique
      VariableStatus(): isReeliminated(false), isAboveRelinThreshold(false), isRelinearizeInvolved(false),
          isRelinearized(false), isObserved(false), isNew(false), inRootClique(false) {}
    };

    /** The status of each variable during this update, see VariableStatus.
     */
    FastMap<Key, VariableStatus> variableStatus;
  };

  /** Detailed results, if enabled by ISAM2Params::enableDetailedResults.  See
   * Detail for information about the results data stored here. */
  boost::optional<DetailedResults> detail;
};

struct ISAM2Clique : public BayesTreeCliqueBase<ISAM2Clique, GaussianConditional> {

  typedef ISAM2Clique This;
  typedef BayesTreeCliqueBase<This,GaussianConditional> Base;
  typedef boost::shared_ptr<This> shared_ptr;
  typedef boost::weak_ptr<This> weak_ptr;
  typedef GaussianConditional ConditionalType;
  typedef ConditionalType::shared_ptr sharedConditional;

  Base::FactorType::shared_ptr cachedFactor_;
  Vector gradientContribution_;

  /** Construct from a conditional */
  ISAM2Clique(const sharedConditional& conditional) : Base(conditional) {
    throw runtime_error("ISAM2Clique should always be constructed with the elimination result constructor"); }

  /** Construct from an elimination result */
  ISAM2Clique(const std::pair<sharedConditional, boost::shared_ptr<ConditionalType::FactorType> >& result) :
    Base(result.first), cachedFactor_(result.second), gradientContribution_(result.first->get_R().cols() + result.first->get_S().cols()) {
    // Compute gradient contribution
    const ConditionalType& conditional(*result.first);
    // Rewrite -(R * P')'*d   as   -(d' * R * P')'   for computational speed reasons
    gradientContribution_ << -(conditional.get_d().transpose() * conditional.get_R() * conditional.permutation().transpose()).transpose(),
        -conditional.get_S().transpose() * conditional.get_d();
  }

  /** Produce a deep copy, copying the cached factor and gradient contribution */
  shared_ptr clone() const {
    shared_ptr copy(new ISAM2Clique(make_pair(
        sharedConditional(new ConditionalType(*Base::conditional_)),
        cachedFactor_ ? cachedFactor_->clone() : Base::FactorType::shared_ptr())));
    copy->gradientContribution_ = gradientContribution_;
    return copy;
  }

  /** Access the cached factor */
   Base::FactorType::shared_ptr& cachedFactor() { return cachedFactor_; }

  /** Access the gradient contribution */
  const Vector& gradientContribution() const { return gradientContribution_; }

  bool equals(const This& other, double tol=1e-9) const {
    return Base::equals(other) && ((!cachedFactor_ && !other.cachedFactor_) || (cachedFactor_ && other.cachedFactor_ && cachedFactor_->equals(*other.cachedFactor_, tol)));
  }

  /** print this node */
  void print(const std::string& s = "") const {
    Base::print(s);
    if(cachedFactor_)
      cachedFactor_->print(s + "Cached: ");
    else
      cout << s << "Cached empty" << endl;
    if(gradientContribution_.rows() != 0)
      gtsam::print(gradientContribution_, "Gradient contribution: ");
  }

  void permuteWithInverse(const Permutation& inversePermutation) {
    if(cachedFactor_) cachedFactor_->permuteWithInverse(inversePermutation);
    Base::permuteWithInverse(inversePermutation);
  }

  bool permuteSeparatorWithInverse(const Permutation& inversePermutation) {
    bool changed = Base::permuteSeparatorWithInverse(inversePermutation);
    if(changed) if(cachedFactor_) cachedFactor_->permuteWithInverse(inversePermutation);
    return changed;
  }

private:
  /** Serialization function */
  friend class boost::serialization::access;
  template<class ARCHIVE>
  void serialize(ARCHIVE & ar, const unsigned int version) {
    ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Base);
    ar & BOOST_SERIALIZATION_NVP(cachedFactor_);
    ar & BOOST_SERIALIZATION_NVP(gradientContribution_);
  }
};

/**
 * @ingroup ISAM2
 * Implementation of the full ISAM2 algorithm for incremental nonlinear optimization.
 *
 * The typical cycle of using this class to create an instance by providing ISAM2Params
 * to the constructor, then add measurements and variables as they arrive using the update()
 * method.  At any time, calculateEstimate() may be called to obtain the current
 * estimate of all variables.
 *
 */
class ISAM2: public BayesTree<GaussianConditional, ISAM2Clique> {

protected:

  /** The current linearization point */
  Values theta_;

  /** VariableIndex lets us look up factors by involved variable and keeps track of dimensions */
  VariableIndex variableIndex_;

  /** The linear delta from the last linear solution, an update to the estimate in theta */
  VectorValues deltaUnpermuted_;

  /** The permutation through which the deltaUnpermuted_ is
   * referenced.
   *
   * Permuting Vector entries would be slow, so for performance we
   * instead maintain this permutation through which we access the linear delta
   * indirectly
   *
   * This is \c mutable because it is a "cached" variable - it is not updated
   * until either requested with getDelta() or calculateEstimate(), or needed
   * during update() to evaluate whether to relinearize variables.
   */
  mutable Permuted<VectorValues> delta_;

  VectorValues deltaNewtonUnpermuted_;
  mutable Permuted<VectorValues> deltaNewton_;
  VectorValues RgProdUnpermuted_;
  mutable Permuted<VectorValues> RgProd_;
  mutable bool deltaDoglegUptodate_;

  /** Indicates whether the current delta is up-to-date, only used
   * internally - delta will always be updated if necessary when it is
   * requested with getDelta() or calculateEstimate().
   *
   * This is \c mutable because it is used internally to not update delta_
   * until it is needed.
   */
  mutable bool deltaUptodate_;

  /** A cumulative mask for the variables that were replaced and have not yet
   * been updated in the linear solution delta_, this is only used internally,
   * delta will always be updated if necessary when requested with getDelta()
   * or calculateEstimate().
   *
   * This does not need to be permuted because any change in variable ordering
   * that would cause a permutation will also mark variables as needing to be
   * updated in this mask.
   *
   * This is \c mutable because it is used internally to not update delta_
   * until it is needed.
   */
  mutable std::vector<bool> deltaReplacedMask_;

  /** All original nonlinear factors are stored here to use during relinearization */
  NonlinearFactorGraph nonlinearFactors_;

  /** The current linear factors, which are only updated as needed */
  mutable GaussianFactorGraph linearFactors_;

  /** The current elimination ordering Symbols to Index (integer) keys.
   *
   * We keep it up to date as we add and reorder variables.
   */
  Ordering ordering_;

  /** The current parameters */
  ISAM2Params params_;

  /** The current Dogleg Delta (trust region radius) */
  mutable boost::optional<double> doglegDelta_;

  /** The inverse ordering, only used for creating ISAM2Result::DetailedResults */
  boost::optional<Ordering::InvertedMap> inverseOrdering_;

private:
#ifndef NDEBUG
  std::vector<bool> lastRelinVariables_;
#endif

public:

  typedef ISAM2 This; ///< This class
  typedef BayesTree<GaussianConditional,ISAM2Clique> Base; ///< The BayesTree base class

  /** Create an empty ISAM2 instance */
  ISAM2(const ISAM2Params& params);

  /** Create an empty ISAM2 instance using the default set of parameters (see ISAM2Params) */
  ISAM2();

  /** Copy constructor */
  ISAM2(const ISAM2& other);

  /** Assignment operator */
  ISAM2& operator=(const ISAM2& rhs);

  typedef Base::Clique Clique; ///< A clique
  typedef Base::sharedClique sharedClique; ///< Shared pointer to a clique
  typedef Base::Cliques Cliques; ///< List of Clique typedef from base class

  /**
   * Add new factors, updating the solution and relinearizing as needed.
   *
   * Optionally, this function remove existing factors from the system to enable
   * behaviors such as swapping existing factors with new ones.
   *
   * Add new measurements, and optionally new variables, to the current system.
   * This runs a full step of the ISAM2 algorithm, relinearizing and updating
   * the solution as needed, according to the wildfire and relinearize
   * thresholds.
   *
   * @param newFactors The new factors to be added to the system
   * @param newTheta Initialization points for new variables to be added to the system.
   * You must include here all new variables occuring in newFactors (which were not already
   * in the system).  There must not be any variables here that do not occur in newFactors,
   * and additionally, variables that were already in the system must not be included here.
   * @param removeFactorIndices Indices of factors to remove from system
   * @param force_relinearize Relinearize any variables whose delta magnitude is sufficiently
   * large (Params::relinearizeThreshold), regardless of the relinearization interval
   * (Params::relinearizeSkip).
   * @return An ISAM2Result struct containing information about the update
   */
  ISAM2Result update(const NonlinearFactorGraph& newFactors = NonlinearFactorGraph(), const Values& newTheta = Values(),
      const FastVector<size_t>& removeFactorIndices = FastVector<size_t>(),
      const boost::optional<FastMap<Key,int> >& constrainedKeys = boost::none,
      bool force_relinearize = false);

  /** Access the current linearization point */
  const Values& getLinearizationPoint() const {return theta_;}

  /** Compute an estimate from the incomplete linear delta computed during the last update.
   * This delta is incomplete because it was not updated below wildfire_threshold.  If only
   * a single variable is needed, it is faster to call calculateEstimate(const KEY&).
   */
  Values calculateEstimate() const;

  /** Compute an estimate for a single variable using its incomplete linear delta computed
   * during the last update.  This is faster than calling the no-argument version of
   * calculateEstimate, which operates on all variables.
   * @param key
   * @return
   */
  template<class VALUE>
  VALUE calculateEstimate(Key key) const;

  /// @name Public members for non-typical usage
  //@{

  /** Internal implementation functions */
  struct Impl;

  /** Compute an estimate using a complete delta computed by a full back-substitution.
   */
  Values calculateBestEstimate() const;

  /** Access the current delta, computed during the last call to update */
  const Permuted<VectorValues>& getDelta() const;

  /** Access the set of nonlinear factors */
  const NonlinearFactorGraph& getFactorsUnsafe() const { return nonlinearFactors_; }

  /** Access the current ordering */
  const Ordering& getOrdering() const { return ordering_; }

  size_t lastAffectedVariableCount;
  size_t lastAffectedFactorCount;
  size_t lastAffectedCliqueCount;
  size_t lastAffectedMarkedCount;
  mutable size_t lastBacksubVariableCount;
  size_t lastNnzTop;

  ISAM2Params params() const { return params_; }

  //@}

private:

  FastList<size_t> getAffectedFactors(const FastList<Index>& keys) const;
  FactorGraph<GaussianFactor>::shared_ptr relinearizeAffectedFactors(const FastList<Index>& affectedKeys, const FastSet<Index>& relinKeys) const;
  GaussianFactorGraph getCachedBoundaryFactors(Cliques& orphans);

  boost::shared_ptr<FastSet<Index> > recalculate(const FastSet<Index>& markedKeys, const FastSet<Index>& relinKeys,
      const FastVector<Index>& observedKeys,
      const boost::optional<FastMap<Index,int> >& constrainKeys, ISAM2Result& result);
  //	void linear_update(const GaussianFactorGraph& newFactors);
  void updateDelta(bool forceFullSolve = false) const;

  friend void optimizeInPlace(const ISAM2&, VectorValues&);
  friend void optimizeGradientSearchInPlace(const ISAM2&, VectorValues&);

}; // ISAM2

/** Get the linear delta for the ISAM2 object, unpermuted the delta returned by ISAM2::getDelta() */
VectorValues optimize(const ISAM2& isam);

/** Get the linear delta for the ISAM2 object, unpermuted the delta returned by ISAM2::getDelta() */
void optimizeInPlace(const ISAM2& isam, VectorValues& delta);

/// Optimize the BayesTree, starting from the root.
/// @param replaced Needs to contain
/// all variables that are contained in the top of the Bayes tree that has been
/// redone.
/// @param delta The current solution, an offset from the linearization
/// point.
/// @param threshold The maximum change against the PREVIOUS delta for
/// non-replaced variables that can be ignored, ie. the old delta entry is kept
/// and recursive backsubstitution might eventually stop if none of the changed
/// variables are contained in the subtree.
/// @return The number of variables that were solved for
template<class CLIQUE>
int optimizeWildfire(const boost::shared_ptr<CLIQUE>& root,
    double threshold, const std::vector<bool>& replaced, Permuted<VectorValues>& delta);

/**
 * Optimize along the gradient direction, with a closed-form computation to
 * perform the line search.  The gradient is computed about \f$ \delta x=0 \f$.
 *
 * This function returns \f$ \delta x \f$ that minimizes a reparametrized
 * problem.  The error function of a GaussianBayesNet is
 *
 * \f[ f(\delta x) = \frac{1}{2} |R \delta x - d|^2 = \frac{1}{2}d^T d - d^T R \delta x + \frac{1}{2} \delta x^T R^T R \delta x \f]
 *
 * with gradient and Hessian
 *
 * \f[ g(\delta x) = R^T(R\delta x - d), \qquad G(\delta x) = R^T R. \f]
 *
 * This function performs the line search in the direction of the
 * gradient evaluated at \f$ g = g(\delta x = 0) \f$ with step size
 * \f$ \alpha \f$ that minimizes \f$ f(\delta x = \alpha g) \f$:
 *
 * \f[ f(\alpha) = \frac{1}{2} d^T d + g^T \delta x + \frac{1}{2} \alpha^2 g^T G g \f]
 *
 * Optimizing by setting the derivative to zero yields
 * \f$ \hat \alpha = (-g^T g) / (g^T G g) \f$.  For efficiency, this function
 * evaluates the denominator without computing the Hessian \f$ G \f$, returning
 *
 * \f[ \delta x = \hat\alpha g = \frac{-g^T g}{(R g)^T(R g)} \f]
 */
VectorValues optimizeGradientSearch(const ISAM2& isam);

/** In-place version of optimizeGradientSearch requiring pre-allocated VectorValues \c x */
void optimizeGradientSearchInPlace(const ISAM2& isam, VectorValues& grad);

/// calculate the number of non-zero entries for the tree starting at clique (use root for complete matrix)
template<class CLIQUE>
int calculate_nnz(const boost::shared_ptr<CLIQUE>& clique);

/**
 * Compute the gradient of the energy function,
 * \f$ \nabla_{x=x_0} \left\Vert \Sigma^{-1} R x - d \right\Vert^2 \f$,
 * centered around \f$ x = x_0 \f$.
 * The gradient is \f$ R^T(Rx-d) \f$.
 * This specialized version is used with ISAM2, where each clique stores its
 * gradient contribution.
 * @param bayesTree The Gaussian Bayes Tree $(R,d)$
 * @param x0 The center about which to compute the gradient
 * @return The gradient as a VectorValues
 */
VectorValues gradient(const ISAM2& bayesTree, const VectorValues& x0);

/**
 * Compute the gradient of the energy function,
 * \f$ \nabla_{x=0} \left\Vert \Sigma^{-1} R x - d \right\Vert^2 \f$,
 * centered around zero.
 * The gradient about zero is \f$ -R^T d \f$.  See also gradient(const GaussianBayesNet&, const VectorValues&).
 * This specialized version is used with ISAM2, where each clique stores its
 * gradient contribution.
 * @param bayesTree The Gaussian Bayes Tree $(R,d)$
 * @param [output] g A VectorValues to store the gradient, which must be preallocated, see allocateVectorValues
 * @return The gradient as a VectorValues
 */
void gradientAtZero(const ISAM2& bayesTree, VectorValues& g);

} /// namespace gtsam

#include <gtsam/nonlinear/ISAM2-inl.h>
#include <gtsam/nonlinear/ISAM2-impl.h>
