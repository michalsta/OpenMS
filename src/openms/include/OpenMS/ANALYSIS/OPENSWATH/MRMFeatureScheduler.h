// --------------------------------------------------------------------------
//                   OpenMS -- Open-Source Mass Spectrometry
// --------------------------------------------------------------------------
// Copyright The OpenMS Team -- Eberhard Karls University Tuebingen,
// ETH Zurich, and Freie Universitaet Berlin 2002-2018.
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
// $Maintainer: Douglas McCloskey, Pasquale Domenico Colaianni $
// $Authors: Douglas McCloskey, Pasquale Domenico Colaianni $
// --------------------------------------------------------------------------

#pragma once

#include <OpenMS/config.h> // OPENMS_DLLAPI
#include <OpenMS/DATASTRUCTURES/Param.h>
#include <OpenMS/DATASTRUCTURES/DefaultParamHandler.h>
#include <OpenMS/ANALYSIS/OPENSWATH/MRMFeatureSelector.h>
#include <OpenMS/KERNEL/FeatureMap.h>
#include <map>
#include <vector>

namespace OpenMS
{
  class OPENMS_DLLAPI MRMFeatureScheduler
  {
public:
    MRMFeatureScheduler() = default;
    ~MRMFeatureScheduler() = default;

    /**
      Structure to easily feed the parameters to the `MRMFeatureSelector` derived classes
    */
    struct SelectorParameters
    {
      SelectorParameters() = default;

      SelectorParameters(
        Int nn,
        bool lw,
        bool stg,
        Int swl,
        Int ssl,
        String& vt,
        double ot,
        std::map<String,String>& sw
      ) :
        nn_threshold(nn),
        locality_weight(lw),
        select_transition_group(stg),
        segment_window_length(swl),
        segment_step_length(ssl),
        variable_type(vt),
        optimal_threshold(ot),
        score_weights(sw) {}

      Int    nn_threshold            = 4;
      bool   locality_weight         = false;
      bool   select_transition_group = true;
      Int    segment_window_length   = 8;
      Int    segment_step_length     = 4;
      String variable_type           = "continuous";
      double optimal_threshold       = 0.5;
      std::map<String, String> score_weights;
    };

    /**
      Calls `feature_selector.select_MRMFeature()` feeding it the parameters found in `parameters_`.
      It calls said method `parameters_.size()` times, using the result of each cycle as input
      for the next cycle.

      @param[in] feature_selector Base class for the feature selector to use
      @param[in] features Input features
      @param[out] selected_features Selected features
    */
    void schedule_MRMFeatures(MRMFeatureSelector& feature_selector, const FeatureMap& features, FeatureMap& selected_features) const;

    /// Calls `schedule_MRMFeatures()` using a `MRMFeatureSelectorScore` selector
    void schedule_MRMFeaturesScore(const FeatureMap& features, FeatureMap& selected_features) const;

    /// Calls `schedule_MRMFeatures()` using a `MRMFeatureSelectorQMIP` selector
    void schedule_MRMFeaturesQMIP(const FeatureMap& features, FeatureMap& selected_features) const;

    /// Setter for the scheduler's parameters
    void setSchedulerParameters(const std::vector<SelectorParameters>& parameters);

    /// Getter for the scheduler's parameters
    std::vector<SelectorParameters>& getSchedulerParameters(void);

private:
    /// Parameters for a single call to the scheduler. All elements will be consumed.
    std::vector<SelectorParameters> parameters_;
  };
}
