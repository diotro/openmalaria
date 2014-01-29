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

#ifndef Hmod_WithinHost_Interface
#define Hmod_WithinHost_Interface

#include "Global.h"
#include "Monitoring/Survey.h"
#include "WithinHost/Pathogenesis/State.h"

#include <list>

using namespace std;

class UnittestUtil;

namespace OM {
namespace WithinHost {

/**
 * Interface to the within-host models. These models encapsulate the infections
 * and related immunity factors of a single human, starting with infection
 * (i.e. assuming successful innoculation), including some drug action code,
 * and outputting parasite densities.
 */
class WHInterface {
public:
    /// @brief Static methods
    //@{
    /// Initialise static parameters
    static void init();

    /// Create an instance using the appropriate model
    static WHInterface* createWithinHostModel ();
    //@}

    /// @brief Constructors, destructors and checkpointing functions
    //@{
    WHInterface();
    /** Second step of initialisation (could be combined with constructor, but
     * for the moment separate to avoid changing the order of random number
     * samples). */
    virtual void setComorbidityFactor( double factor ) =0;
    virtual ~WHInterface();

    /// Checkpointing
    template<class S>
    void operator& (S& stream) {
        checkpoint (stream);
    }
    //@}

    /// @returns true if host has patent parasites
    virtual bool summarize(Monitoring::Survey& survey, Monitoring::AgeGroup ageGroup) =0;

    /// Create a new infection within this human
    virtual void importInfection() =0;
    /** Conditionally clears all infections. Not used with the PK/PD model.
     *
     * If IPT isn't present, it just calls clearAllInfections(); otherwise it
     * uses IPT code to determine whether to clear all infections or do nothing
     * (isSevere is only used in the IPT case). */
    virtual void clearInfections (bool isSevere);

    /** Medicate drugs (wraps drug's medicate).
     *
     * @param drugAbbrev	abbrevation of drug name (e.g. CQ, MF)
     * @param qty		Quantity of drug to administer in mg
     * @param time		Time relative to beginning of timestep to medicate at, in days (less than 1 day)
     * @param duration Duration in days. 0 or NaN indicate oral treatment.
     * @param bodyMass	Weight of human in kg
     */
    virtual void medicate(string drugAbbrev, double qty, double time, double duration, double bodyMass);

    /** Add new infections and update the parasite densities of existing
     * infections. Also update immune status.
     *
     * @param nNewInfs Number of inoculations this time-step
     * @param ageInYears Age of human
     * @param BSVEfficacy Efficacy of blood-stage vaccine */
    virtual void update(int nNewInfs, double ageInYears, double BSVEfficacy) =0;

    inline bool parasiteDensityDetectible() const {
        return totalDensity > detectionLimit;
    }

    // TODO: these should not be exposed outsite the withinhost models:
    inline double getTotalDensity() const {
        return totalDensity;
    }
    
    /** Use the pathogenesis model to determine, based on infection status
     * and random draw, this person't morbidity.
     * 
     * @param ageYears Age of human host in years
     */
    virtual Pathogenesis::State determineMorbidity( double ageYears ) =0;

    ///@brief Only do anything when IPT is present:
    //@{
    /// Continuous deployment for IPT
    virtual void continuousIPT (Monitoring::AgeGroup ageGroup, bool inCohort);
    /// Timed deployment for IPT
    virtual void timedIPT (Monitoring::AgeGroup ageGroup, bool inCohort);
    /// Last IPTi dose recent enough to give protection?
    virtual bool hasIPTiProtection (TimeStep maxInterventionAge) const;
    //@}

    /// Called to effect some penalty on immunity − but what? Please document.
    virtual void immunityPenalisation() =0;
    /// Special intervention: clears all immunity
    virtual void immuneSuppression() =0;

    // TODO: these shouldn't have to be exposed (perhaps use summarize to report the data):
    virtual double getCumulativeh() const =0;
    virtual double getCumulativeY() const =0;

    /** The maximum number of infections a human can have. The only real reason
     * for this limit is to prevent incase bad input from causing the number of
     * infections to baloon stupidly.
     *
     * Exact constraint is: _MOI <= MAX_INFECTIONS. */
    static const int MAX_INFECTIONS = 21;

protected:

    /** For summarizing:
     * @returns Total number of infections.
     * @param patentInfections In-out param: incremented for every patent infection */
    virtual int countInfections (int& patentInfections) =0;

    virtual void checkpoint (istream& stream);
    virtual void checkpoint (ostream& stream);

    /** Literally just removes all infections in an individual.
     *
     * Normally clearInfections() would be called instead, which, when IPT is not
     * active, just calls this function (although this needs to be changed for
     * PK_PD integration). */
    virtual void clearAllInfections() =0;

    /// Multiplicity of infection
    int numInfs;

    //TODO: should these two parameters be in this class? Should the pathogenesis model be part of the withinhost model?
    /// Total asexual blood stage density (sum of density of infections).
    double totalDensity;

    /*
    The detection limit (in parasites/ul) is currently the same for PCR and for microscopy
    TODO: in fact the detection limit in Garki should be the same as the PCR detection limit
    The density bias allows the detection limit for microscopy to be higher for other sites
    */
    static double detectionLimit;

    friend class ::UnittestUtil;
};

}
}
#endif