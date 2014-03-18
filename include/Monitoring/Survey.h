/* This file is part of OpenMalaria.
 * 
 * Copyright (C) 2005-2014 Swiss Tropical and Public Health Institute
 * Copyright (C) 2005-2014 Liverpool School Of Tropical Medicine
 * 
 * OpenMalaria is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef Hmod_Survey
#define Hmod_Survey

#include "Monitoring/SurveyMeasure.h"
#include "Global.h"
#include "util/errors.h"
#include "util/checkpoint_containers.h"
#include <bitset>
#include <map>
#include <boost/multi_array.hpp>

namespace scnXml{ class Monitoring; }
namespace OM {
    namespace Host {
        class Human;
    }
namespace Monitoring {
    using boost::multi_array;

/// Encapsulate report measure codes
namespace Report {
    /** Measures which are reported as integers
     * 
     * Note: for timed/continuous deployment pairs, the continuous version
     * is always the timed version + 1. */
    enum IntReportMeasures{
        MI_HOSTS,
        MI_INFECTED_HOSTS,
        MI_PATENT_HOSTS,
        MI_INFECTIONS,
        MI_PATENT_INFECTIONS,
        MI_TREATMENTS_1,
        MI_TREATMENTS_2,
        MI_TREATMENTS_3,
        MI_UNCOMPLICATED_EPISODES,
        MI_SEVERE_EPISODES,
        MI_SEQUELAE,
        MI_HOSPITAL_DEATHS,
        MI_INDIRECT_DEATHS,
        MI_DIRECT_DEATHS,
        MI_VACCINATION_TIMED,
        MI_VACCINATION_CTS,
        MI_HOSPITAL_RECOVERIES,
        MI_HOSPITAL_SEQUELAE,
        MI_NON_MALARIA_FEVERS,
        MI_NEW_INFECTIONS,
        MI_ITN_TIMED,
        MI_ITN_CTS,
        MI_IRS_TIMED,
        MI_IRS_CTS,
        MI_GVI_TIMED,
        MI_GVI_CTS,
        MI_MDA_TIMED,
        MI_MDA_CTS /* "mass" drug administration via EPI/schools */,
        MI_SCREENING_TIMED,
        MI_SCREENING_CTS,
        MI_NMF_DEATHS,
        MI_NMF_TREATMENTS /* also known as antibiotics */,
        MI_FIRST_DAY_DEATHS,
        MI_HOSPITAL_FIRST_DAY_DEATHS,
        //TODO: cohorts should be handled independently, not as "in cohort"/"not in a cohort"
        MI_NUM_ADDED_COHORT,
        MI_NUM_REMOVED_COHORT,
        MI_NUM  // must be last; not a measure to report
    };
    /// Measures which are reported as doubles
    enum DblReportMeasures{
        MD_EXPECTED_INFECTED,
        MD_LOG_PYROGENIC_THRESHOLD,
        MD_LOG_DENSITY,
        MD_PYROGENIC_THRESHOLD,
        MD_NUM  // must be last; not a measure to report
    };
}
/** Wrap an IntReportMeasures to enforce initialisation. */
struct ReportMeasureI{
    /* implicit */ ReportMeasureI( Report::IntReportMeasures m ) : code( m ) {}
    Report::IntReportMeasures code;
};
/** Wrap an DblReportMeasures to enforce initialisation. */
struct ReportMeasureD{
    /* implicit */ ReportMeasureD( Report::DblReportMeasures m ) : code( m ) {}
    Report::DblReportMeasures code;
};

/**
 * Included for type-saftey: don't allow implicit double->int conversions.
 *
 * Incindentally, the constructor can be used implicitly for implicit
 * conversion doing the right thing.
 * 
 * Don't use _this_ class for other index/age-group types. */
class AgeGroup {
  public:
    AgeGroup () : index(0) {}
    
    /** Update age-group. Assumes age only increases (per instance).
     *
     * If called regularly, should be O(1); worst case is O(_upperbound.size()). */
    void update (double ageYears);
    
    /// Checkpointing
    template<class S>
    void operator& (S& stream) {
        index & stream;
    }
    
    /** Get the represented index. */
    inline size_t i () {
        return index;
    }
    
    /// Get the total number of age categories (inc. one for indivs. not in any
    /// category given in XML).
    static inline size_t getNumGroups () {
        if( _upperbound.size() == 0 ) throw TRACED_EXCEPTION_DEFAULT( "not yet initialised" );
        return _upperbound.size();
    }
    
private:
    size_t index;
    
    /// Initialize _lowerbound and _upperbound
    static void init (const scnXml::Monitoring& monitoring);
    
    //BEGIN Static parameters only set by init()
    /// Lower boundary of the youngest agegroup
    static double _lowerbound;
    /** Upper boundary of agegroups, in years.
     *
     * These are age-groups given in XML plus one with no upper limit for
     * individuals outside other bounds. */
    static vector<double> _upperbound;
    //END
    
    friend class Survey;
};

/// Data struct for a single survey.
class Survey {
public:
    // Constructor used by SurveysType. Call allocate() explicitly for allocation.
    Survey();
private:
    
    ///@brief Static members (options from XML). Parameters only set by init().
    //@{
    /// Initialize static parameters.
    static void init(const scnXml::Monitoring& monitoring);
    
    /// Encoding of which summary options are active in XML is converted into
    /// this array for easier reading (and to make changing encoding within XML easier).
    static bitset<SM::NUM_SURVEY_OPTIONS> active;
    //@}
  
public:
    /**
     * Report some integer number of events, adding the number to a total.
     * 
     * @param ageGroup Age group of host
     * @param val Number of events (added to total)
     * @returns (*this) object to allow chain calling
     */
    Survey& addInt( ReportMeasureI measure, AgeGroup ageGroup, int val ){
        if( static_cast<size_t>(measure.code) >= reportsIntAge.shape()[0] ||
            ageGroup.i() >= reportsIntAge.shape()[1] ){
            cout << "Index out of bounds:\n"
                "survey\t" << static_cast<void*>(this)
                << "\nalloc\t" << reportsIntAge.shape()[0] << "\t" << reportsIntAge.shape()[1]
                << "\nindex\t" << measure.code << "\t" << ageGroup.i() << endl;
        }
        reportsIntAge[measure.code][ageGroup.i()] += val;
        return *this;
    }
    /**
     * Report some quantity (double), adding the quantity to a total.
     * 
     * @param ageGroup Age group of host
     * @param val Quantity (added to total)
     * @returns (*this) object to allow chain calling
     */
    Survey& addDouble( ReportMeasureD measure, AgeGroup ageGroup, double val ){
        if( static_cast<size_t>(measure.code) >= reportsDblAge.shape()[0] ||
            ageGroup.i() >= reportsDblAge.shape()[1] ){
            cout << "Index out of bounds:\n"
                "survey\t" << static_cast<void*>(this)
                << "\nalloc\t" << reportsDblAge.shape()[0] << "\t" << reportsDblAge.shape()[1]
                << "\nindex\t" << measure.code << "\t" << ageGroup.i() << endl;
        }
        reportsDblAge[measure.code][ageGroup.i()] += val;
        return *this;
    }
    
  void setAnnualAverageKappa(double kappa) {
    _annualAverageKappa = kappa;
  }
  void setInfectiousnessToMosq(double value) {
    _infectiousnessToMosq = value;
  }
  
  void setInoculationsPerAgeGroup (vector<double>& v) {
    _inoculationsPerAgeGroup = v;	// copies v, not just its reference
  }
  void report_Clinical_RDTs (int num) {
      _numClinical_RDTs += num;
  }
  void report_Clinical_DrugUsage (string abbrev, double qty) {
      // Insert the pair (abbrev, 0.0) if not there, get an iterator to it, and increment it's second param (quantity) by qty
      (*((_sumClinical_DrugUsage.insert(make_pair(abbrev, 0.0))).first)).second += qty;
  }
  void report_Clinical_DrugUsageIV (string abbrev, double qty) {
      // Insert the pair (abbrev, 0.0) if not there, get an iterator to it, and increment it's second param (quantity) by qty
      (*((_sumClinical_DrugUsageIV.insert(make_pair(abbrev, 0.0))).first)).second += qty;
  }
  void report_Clinical_Microscopy (int num) {
      _numClinical_Microscopy += num;
  }
  void set_Vector_Nv0 (string key, double v) {
    data_Vector_Nv0[key] = v;
  }
  void set_Vector_Nv (string key, double v) {
    data_Vector_Nv[key] = v;
  }
  void set_Vector_Ov (string key, double v) {
    data_Vector_Ov[key] = v;
  }
  void set_Vector_Sv (string key, double v) {
    data_Vector_Sv[key] = v;
  }
  void setInputEIR (double v) {
    _inputEIR = v;
  }
  void setSimulatedEIR (double v) {
    _simulatedEIR = v;
  }
  
  /// Checkpointing
  template<class S>
  void operator& (S& stream) {
      checkpoint( stream );
    _infectiousnessToMosq & stream;
    _annualAverageKappa & stream;
    _inoculationsPerAgeGroup & stream;
    data_Vector_Nv0 & stream;
    data_Vector_Nv & stream;
    data_Vector_Ov & stream;
    data_Vector_Sv & stream;
    _inputEIR & stream;
    _simulatedEIR & stream;
    _numClinical_RDTs & stream;
    _sumClinical_DrugUsage & stream;
    _sumClinical_DrugUsageIV & stream;
    _numClinical_Microscopy & stream;
  }
  
private:
  /** Resizes all vectors, allocating memory.
   * 
   * This is a separate initialisation step to make allocation explicit and
   * avoid accidental allocations when manipulating containers of Survey
   * elements. */
  void allocate ();
  
  /** Write out arrays
   * @param outputFile Stream to write to
   * @param survey Survey number (starting from 1) */
  void writeSummaryArrays (ostream& outputFile, int survey);
  
  // atomic data:
  double _infectiousnessToMosq;
  double _annualAverageKappa;
  
  // first index is the measure (IntReportMeasures), second is age group:
  typedef multi_array<int, 2> ReportsIntAgeT;
  ReportsIntAgeT reportsIntAge;
  typedef multi_array<double, 2> ReportsDblAgeT;
  ReportsDblAgeT reportsDblAge;
  // data, per AgeGroup:
  vector<double> _inoculationsPerAgeGroup;
  
    // data, per vector species:
    map<string,double> data_Vector_Nv0;
    map<string,double> data_Vector_Nv;
    map<string,double> data_Vector_Ov;
    map<string,double> data_Vector_Sv;
    double _inputEIR;
    double _simulatedEIR;
    
    int _numClinical_RDTs;
    map<string,double> _sumClinical_DrugUsage;
    map<string,double> _sumClinical_DrugUsageIV;
    int _numClinical_Microscopy;
    
    void checkpoint( istream& stream );
    void checkpoint( ostream& stream) const;
    
    friend class SurveysType;
};

/** Line end character. Use Unix line endings to save a little size. */
const char lineEnd = '\n';

} }
#endif
