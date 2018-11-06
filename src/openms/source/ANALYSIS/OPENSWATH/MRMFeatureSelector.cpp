// --------------------------------------------------------------------------
//                   OpenMS -- Open-Source Mass Spectrometry
// --------------------------------------------------------------------------
// Copyright The OpenMS Team -- Eberhard Karls University Tuebingen,
// ETH Zurich, and Freie Universitaet Berlin 2002-2017.
//
// This software is released under a three-clause BSD license:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of any author or any participating institution
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
// For a full list of authors, refer to the file AUTHORS.
// --------------------------------------------------------------------------
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL ANY OF THE AUTHORS OR THE CONTRIBUTING
// INSTITUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// --------------------------------------------------------------------------
// $Maintainer: Douglas McCloskey, Pasquale Domenico Colaianni, Svetlana Kutuzova $
// $Authors: Douglas McCloskey, Pasquale Domenico Colaianni, Svetlana Kutuzova $
// --------------------------------------------------------------------------

#include <algorithm>
#include <unordered_set>
#include <OpenMS/KERNEL/FeatureMap.h>
#include <OpenMS/DATASTRUCTURES/LPWrapper.h>
#include <OpenMS/ANALYSIS/OPENSWATH/MRMFeatureSelector.h>


namespace OpenMS
{
  Int MRMFeatureSelector::_addVariable(
    LPWrapper& problem,
    const String& name,
    const bool bounded,
    const double obj
  ) const
  {
    const Int index = problem.addColumn();

    if (bounded) {
      problem.setColumnBounds(index, 0, 1, LPWrapper::DOUBLE_BOUNDED);
    } else {
      problem.setColumnBounds(index, 0, 1, LPWrapper::UNBOUNDED);
    }

    problem.setColumnName(index, name);

    if (getVariableType() == s_integer) {
      problem.setColumnType(index, LPWrapper::INTEGER);
    } else if (getVariableType() == s_continuous) {
      problem.setColumnType(index, LPWrapper::CONTINUOUS);
    } else {
      throw "Variable type not supported\n";
    }

    problem.setObjective(index, obj);
    return index;
  }

  void MRMFeatureSelector::_addConstraint(
    LPWrapper& problem,
    std::vector<Int> indices,
    std::vector<double> values,
    const String& name,
    const double lb,
    const double ub,
    const LPWrapper::Type param
  ) const
  {
    problem.addRow(indices, values, name, lb, ub, param);
  }

  void MRMFeatureSelectorScore::optimize(
    const std::vector<std::pair<double, String>>& time_to_name,
    const std::map< String, std::vector<Feature> >& feature_name_map,
    std::vector<String>& result
  )
  {
    result.clear();
    std::unordered_set<std::string> variables;
    LPWrapper problem;
    problem.setObjectiveSense(LPWrapper::MIN);
    for (const std::pair<double, String>& elem : time_to_name) {
      std::vector<Int> constraints;
      for (const Feature& feature : feature_name_map.at(elem.second)) {
        const String name1 = elem.second + "_" + String(feature.getUniqueId());
        if (variables.count(name1) == 0) {
            constraints.push_back(_addVariable(problem, name1, true, make_score(feature)));
            variables.insert(name1);
        }
      }
      std::vector<double> constraints_values(constraints.size(), 1.0);
      _addConstraint(problem, constraints, constraints_values, elem.second + "_constraint", 1.0, 1.0, LPWrapper::DOUBLE_BOUNDED);
    }
    LPWrapper::SolverParam param;
    problem.solve(param);
    for (Int c = 0; c < problem.getNumberOfColumns(); ++c) {
      if (problem.getColumnValue(c) >= getOptimalThreshold()) {
        result.push_back(problem.getColumnName(c));
      }
    }
  }

  String MRMFeatureSelector::remove_spaces(String str) const
  {
    String::iterator end_pos = std::remove(str.begin(), str.end(), ' ');
    str.erase(end_pos, str.end());
    return std::move(str);
  }

  void MRMFeatureSelectorQMIP::optimize(
    const std::vector<std::pair<double, String>>& time_to_name, // To_list
    const std::map< String, std::vector<Feature> >& feature_name_map, // Tr_dict
    std::vector<String>& result
  )
  {
// std::cout << "START OPTIMIZE" << std::endl;
    result.clear();
    std::unordered_set<std::string> variables; // component_names_1
    LPWrapper problem;
    // problem.setSolver(LPWrapper::SOLVER_GLPK);
    problem.setObjectiveSense(LPWrapper::MIN);
    size_t n_constraints = 0;
    size_t n_variables = 0;
    for (Int cnt1 = 0; static_cast<size_t>(cnt1) < time_to_name.size(); ++cnt1) {
      const size_t start_iter = std::max(cnt1 - nn_threshold_, 0);
      const size_t stop_iter = std::min(static_cast<size_t>(cnt1 + nn_threshold_ + 1), time_to_name.size()); // assuming nn_threshold_ >= -1
      std::vector<Int> constraints;
      const std::vector<Feature> feature_row1 = feature_name_map.at(time_to_name[cnt1].second);

      for (size_t i = 0; i < feature_row1.size(); ++i) {
        const String name1 = time_to_name[cnt1].second + "_" + String(feature_row1[i].getUniqueId());

        if (variables.count(name1) == 0) {
          constraints.push_back(_addVariable(problem, name1, true, 0));
          variables.insert(name1);
          ++n_variables;
        } else {
          constraints.push_back(problem.getColumnIndex(name1));
        }

        double score_1 = make_score(feature_row1[i]);
        const size_t n_score_weights = score_weights_.size();

        if (n_score_weights > 1) {
          score_1 = std::pow(score_1, 1.0 / n_score_weights);
        }

        const Int index1 = problem.getColumnIndex(name1);

        for (size_t cnt2 = start_iter; cnt2 < stop_iter; ++cnt2) {
          if (static_cast<size_t>(cnt1) == cnt2)
            continue;

          const std::vector<Feature> feature_row2 = feature_name_map.at(time_to_name[cnt2].second);
          const double locality_weight = getLocalityWeight() == "true"
            ? 1.0 / (nn_threshold_ - std::abs(static_cast<Int>(start_iter + cnt2) - cnt1) + 1)
            : 1.0;
          const double tr_delta_expected = time_to_name[cnt1].first - time_to_name[cnt2].first;

          for (size_t j = 0; j < feature_row2.size(); ++j) {
            const String name2 = time_to_name[cnt2].second + "_" + String(feature_row2[j].getUniqueId());
            if (variables.count(name2) == 0) {
              _addVariable(problem, name2, true, 0);
              variables.insert(name2);
              ++n_variables;
            }

            const String var_qp_name = time_to_name[cnt1].second + "_" + String(i) + "-" + time_to_name[cnt2].second + "_" + String(j);

            // The two following problem's variables need the variable type to be "continuous"
            // Save current variable type to later set it back to the same value
            const String prev_variable_type = getVariableType();
            setVariableType(s_continuous);
            const Int index_var_qp = _addVariable(problem, var_qp_name, true, 0);
            const Int index_var_abs = _addVariable(problem, var_qp_name + "-ABS", false, 1);
            setVariableType(prev_variable_type);

            const Int index2 = problem.getColumnIndex(name2);

            double score_2 = make_score(feature_row2[j]);
            if (n_score_weights > 1) {
              score_2 = std::pow(score_2, 1.0 / n_score_weights);
            }

            const double tr_delta = feature_row1[i].getRT() - feature_row2[j].getRT();
            const double score = locality_weight * score_1 * score_2 * (tr_delta - tr_delta_expected);

            _addConstraint(problem, {index1, index_var_qp}, {1.0, -1.0}, var_qp_name + "-QP1", 0.0, 1.0, LPWrapper::LOWER_BOUND_ONLY);
            _addConstraint(problem, {index2, index_var_qp}, {1.0, -1.0}, var_qp_name + "-QP2", 0.0, 1.0, LPWrapper::LOWER_BOUND_ONLY);
            _addConstraint(problem, {index1, index2, index_var_qp}, {1.0, 1.0, -1.0}, var_qp_name + "-QP3", 0.0, 1.0, LPWrapper::UPPER_BOUND_ONLY);
            std::vector<Int> indices_abs = {index_var_abs, index_var_qp};
            _addConstraint(problem, indices_abs, {-1.0, score}, var_qp_name + "-obj+", -1.0, 0.0, LPWrapper::UPPER_BOUND_ONLY);
            _addConstraint(problem, indices_abs, {-1.0, -score}, var_qp_name + "-obj-", -1.0, 0.0, LPWrapper::UPPER_BOUND_ONLY);

            n_constraints += 5;
            n_variables += 2;
          }
        }
      }
      std::vector<double> constraints_values(constraints.size(), 1.0);
      _addConstraint(problem, constraints, constraints_values, time_to_name[cnt1].second + "_constraint", 1.0, 1.0, LPWrapper::DOUBLE_BOUNDED);
      ++n_constraints;
// std::cout << "variables: " << variables.size() << "\tconstraints: " << constraints.size() << std::endl;
    }
// std::cout << "n_variables: " << n_variables << "\tn_constraints: " << n_constraints << std::endl;
    LPWrapper::SolverParam param;
    problem.solve(param);
    const double optimal_threshold = getOptimalThreshold();
    for (Int c = 0; c < problem.getNumberOfColumns(); ++c) {
      const String name = problem.getColumnName(c);
      const bool bool_1 = problem.getColumnValue(c) > optimal_threshold - 1e-17;
      const bool bool_2 = variables.count(name);
// std::cout << (bool_1 ? "True" : "False") << " " << (bool_2 ? "True" : "False") << "\t" << std::setprecision(17) << problem.getColumnValue(c) << "\t" << name << std::endl;
// printf("%s %s\t%.21f\t%s\n", (bool_1 ? "True" : "False"), (bool_2 ? "True" : "False"), problem.getColumnValue(c), name.c_str());
      if (bool_1 && bool_2) {
        result.push_back(name);
      }
    }
// std::cout << "result size: " << result.size() << std::endl;
// std::cout << "END OPTIMIZE\n" << std::endl;
  }

  void MRMFeatureSelector::constructToList(
    const FeatureMap& features,
    std::vector<std::pair<double, String>>& time_to_name,
    std::map<String, std::vector<Feature>>& feature_name_map
  ) const
  {
    time_to_name.clear();
    feature_name_map.clear();
    std::unordered_set<std::string> names;
    for (const Feature& feature : features) {
      const String component_group_name = remove_spaces(feature.getMetaValue("PeptideRef").toString());
      const double assay_retention_time = feature.getMetaValue("assay_rt");
      if (names.count(component_group_name) == 0) {
        time_to_name.push_back(std::make_pair(assay_retention_time, component_group_name));
        names.insert(component_group_name);
      }
      if (feature_name_map.count(component_group_name) == 0) {
        feature_name_map[component_group_name] = std::vector<Feature>();
      }
      feature_name_map[component_group_name].push_back(feature);
      if (getSelectTransitionGroup() == "true") {
        continue;
      }
      for (const Feature& subordinate : feature.getSubordinates()) {
        const String component_name = remove_spaces(subordinate.getMetaValue("native_id").toString());
        if (names.count(component_name)) {
          time_to_name.push_back(std::make_pair(assay_retention_time, component_name));
          names.insert(component_name);
        }
        if (feature_name_map.count(component_name) == 0) {
          feature_name_map[component_name] = std::vector<Feature>();
        }
        feature_name_map[component_name].push_back(subordinate);
      }
    }
  }

  void MRMFeatureSelector::select_MRMFeature(const FeatureMap& features, FeatureMap& features_filtered)
  {
    features_filtered.clear();
std::cout << "START SELECT_MRMFEATURE" << std::endl;
std::cout << nn_threshold_ << std::endl;
std::cout << locality_weight_ << std::endl;
std::cout << select_transition_group_ << std::endl;
std::cout << segment_window_length_ << std::endl;
std::cout << segment_step_length_ << std::endl;
std::cout << variable_type_ << std::endl;
std::cout << optimal_threshold_ << std::endl;

std::cout << "features.size(): " << features.size() << std::endl;

    std::vector<std::pair<double, String>> time_to_name;
    std::map<String, std::vector<Feature>> feature_name_map;
    constructToList(features, time_to_name, feature_name_map);

std::cout << time_to_name.size() << std::endl;
std::cout << feature_name_map.size() << std::endl;

    sort(time_to_name.begin(), time_to_name.end());
    Int window_length = getSegmentWindowLength();
    Int step_length = getSegmentStepLength();
    if (window_length == -1 && step_length == -1) {
      window_length = step_length = time_to_name.size();
    }
    size_t n_segments = time_to_name.size() / step_length;
    if (time_to_name.size() % step_length)
      ++n_segments;
    std::vector<String> result_names;

std::cout << "n_segments: " << n_segments << std::endl;

    for (size_t i = 0; i < n_segments; ++i) {
      const size_t start = step_length * i;
      const size_t end = std::min(start + window_length, time_to_name.size());
      const std::vector<std::pair<double, String>> time_slice(time_to_name.begin() + start, time_to_name.begin() + end);
      std::vector<String> result;
      optimize(time_slice, feature_name_map, result);
      result_names.insert(result_names.end(), result.begin(), result.end());
    }
    const std::unordered_set<std::string> result_names_set(result_names.begin(), result_names.end());
    for (const Feature& feature : features) {
      std::vector<Feature> subordinates_filtered;
      for (const Feature& subordinate : feature.getSubordinates()) {
        const String feature_name = getSelectTransitionGroup() == "true"
          ? remove_spaces(feature.getMetaValue("PeptideRef").toString()) + "_" + String(feature.getUniqueId())
          : remove_spaces(subordinate.getMetaValue("native_id").toString()) + "_" + String(feature.getUniqueId());

        if (result_names_set.count(feature_name)) {
          subordinates_filtered.push_back(subordinate);
        }
      }
      if (subordinates_filtered.size()) {
        Feature feature_filtered(feature);
        feature_filtered.setSubordinates(subordinates_filtered);
        features_filtered.push_back(feature_filtered);
      }
    }
std::cout << "features_filtered.size(): " << features_filtered.size() << std::endl;
std::cout << "END SELECT_MRMFEATURE" << std::endl;
  }

  double MRMFeatureSelector::make_score(const Feature& feature) const
  {
    double score_1 = 1.0;
    for (const std::pair<String,String>& score_weight : score_weights_) {
      const String& metavalue_name = score_weight.first;
      const String& lambda_score = score_weight.second;
      if (!feature.metaValueExists(metavalue_name)) {
        LOG_WARN << "make_score(): Metavalue \"" << metavalue_name << "\" not found.";
        continue;
      }
      const double value = weight_func(feature.getMetaValue(metavalue_name), lambda_score);
      if (value > 0.0) {
        score_1 *= value;
      }
    }
    return score_1;
  }

  double MRMFeatureSelector::weight_func(const double score, const String& lambda_score) const
  {
    if (lambda_score == "lambda score: score*1.0") {
      return score * 1.0; // TODO: hardcoded value, but it might actually be different than 1.0
    } else if (lambda_score == "lambda score: 1/score") {
      return 1.0 / score;
    } else if (lambda_score == "lambda score: log(score)") {
      return std::log(score);
    } else if (lambda_score == "lambda score: 1/log(score)") {
      return 1.0 / std::log(score);
    } else if (lambda_score == "lambda score: 1/log10(score)") {
      return 1.0 / std::log10(score);
    } else {
      throw "Case not handled.";
    }
  }

  void MRMFeatureSelector::setNNThreshold(const Int nn_threshold)
  {
    nn_threshold_ = nn_threshold;
  }

  Int MRMFeatureSelector::getNNThreshold() const
  {
    return nn_threshold_;
  }

  void MRMFeatureSelector::setLocalityWeight(const String locality_weight)
  {
    locality_weight_ = locality_weight;
  }

  String MRMFeatureSelector::getLocalityWeight() const
  {
    return locality_weight_;
  }

  void MRMFeatureSelector::setSelectTransitionGroup(const String select_transition_group)
  {
    select_transition_group_ = select_transition_group;
  }

  String MRMFeatureSelector::getSelectTransitionGroup() const
  {
    return select_transition_group_;
  }

  void MRMFeatureSelector::setSegmentWindowLength(const Int segment_window_length)
  {
    segment_window_length_ = segment_window_length;
  }

  Int MRMFeatureSelector::getSegmentWindowLength() const
  {
    return segment_window_length_;
  }

  void MRMFeatureSelector::setSegmentStepLength(const Int segment_step_length)
  {
    segment_step_length_ = segment_step_length;
  }

  Int MRMFeatureSelector::getSegmentStepLength() const
  {
    return segment_step_length_;
  }

  void MRMFeatureSelector::setVariableType(const String& variable_type)
  {
    variable_type_ = variable_type;
  }

  String MRMFeatureSelector::getVariableType() const
  {
    return variable_type_;
  }

  void MRMFeatureSelector::setOptimalThreshold(const double optimal_threshold)
  {
    optimal_threshold_ = optimal_threshold;
  }

  double MRMFeatureSelector::getOptimalThreshold() const
  {
    return optimal_threshold_;
  }

  void MRMFeatureSelector::setScoreWeights(const std::map<String, String>& score_weights)
  {
    score_weights_ = score_weights;
  }

  std::map<String, String> MRMFeatureSelector::getScoreWeights() const
  {
    return score_weights_;
  }
}
