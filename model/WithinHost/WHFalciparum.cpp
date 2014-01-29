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

#include "WithinHost/WHFalciparum.h"
#include "WithinHost/DescriptiveWithinHost.h"
#include "WithinHost/DescriptiveIPTWithinHost.h"
#include "WithinHost/CommonWithinHost.h"
#include "WithinHost/Infection/DummyInfection.h"
#include "WithinHost/Infection/EmpiricalInfection.h"
#include "WithinHost/Infection/MolineauxInfection.h"
#include "WithinHost/Infection/PennyInfection.h"
#include "WithinHost/Pathogenesis/PathogenesisModel.h"
#include "inputData.h"
#include "util/random.h"
#include "util/ModelOptions.h"
#include "util/errors.h"
//using namespace std;

#include <cmath>
#include <boost/format.hpp>


namespace OM {
namespace WithinHost {

using namespace OM::util;

double WHFalciparum::sigma_i;
double WHFalciparum::immPenalty_22;
double WHFalciparum::asexImmRemain;
double WHFalciparum::immEffectorRemain;

// -----  static functions  -----

void WHFalciparum::init() {
    sigma_i=sqrt(InputData.getParameter(Params::SIGMA_I_SQ));
    immPenalty_22=1-exp(InputData.getParameter(Params::IMMUNITY_PENALTY));
    immEffectorRemain=exp(-InputData.getParameter(Params::IMMUNE_EFFECTOR_DECAY));
    asexImmRemain=exp(-InputData.getParameter(Params::ASEXUAL_IMMUNITY_DECAY));
    
    //NOTE: should also call cleanup() on the PathogenesisModel, but it only frees memory which the OS does anyway
    Pathogenesis::PathogenesisModel::init();
}


// -----  Non-static  -----

WHFalciparum::WHFalciparum():
    WHInterface(),
    _cumulativeh(0.0), _cumulativeY(0.0), _cumulativeYlag(0.0),
    timeStepMaxDensity(0.0)
{
    _innateImmSurvFact = exp(-random::gauss(0, sigma_i));
}
void WHFalciparum::setComorbidityFactor(double factor)
{
    pathogenesisModel = auto_ptr<Pathogenesis::PathogenesisModel>(
        Pathogenesis::PathogenesisModel::createPathogenesisModel(factor) );
}

WHFalciparum::~WHFalciparum()
{
}

Pathogenesis::State WHFalciparum::determineMorbidity(double ageYears){
    return pathogenesisModel->determineState( ageYears, timeStepMaxDensity, totalDensity );
}


// -----  immunity  -----

void WHFalciparum::updateImmuneStatus() {
    if (immEffectorRemain < 1) {
        _cumulativeh*=immEffectorRemain;
        _cumulativeY*=immEffectorRemain;
    }
    if (asexImmRemain < 1) {
        _cumulativeh*=asexImmRemain/
                      (1+(_cumulativeh*(1-asexImmRemain)/Infection::cumulativeHstar));
        _cumulativeY*=asexImmRemain/
                      (1+(_cumulativeY*(1-asexImmRemain)/Infection::cumulativeYstar));
    }
    _cumulativeYlag = _cumulativeY;
}

void WHFalciparum::immunityPenalisation() {
    _cumulativeY = _cumulativeYlag - immPenalty_22*(_cumulativeY-_cumulativeYlag);
    if (_cumulativeY < 0) {
        _cumulativeY=0.0;
    }
}


// -----  Summarize  -----

bool WHFalciparum::summarize (Monitoring::Survey& survey, Monitoring::AgeGroup ageGroup) {
    pathogenesisModel->summarize( survey, ageGroup );
    int patentInfections = 0;
    int numInfections = countInfections (patentInfections);
    if (numInfections) {
        survey.reportInfectedHosts(ageGroup,1);
        survey.addToInfections(ageGroup, numInfections);
        survey.addToPatentInfections(ageGroup, patentInfections);
    }
    // Treatments in the old ImmediateOutcomes clinical model clear infections immediately
    // (and are applied after update()); here we report the last calculated density.
    if (parasiteDensityDetectible()) {
        survey.reportPatentHosts(ageGroup, 1);
        survey.addToLogDensity(ageGroup, log(totalDensity));
        return true;
    }
    return false;
}


void WHFalciparum::checkpoint (istream& stream) {
    WHInterface::checkpoint( stream );
    _innateImmSurvFact & stream;
    _cumulativeh & stream;
    _cumulativeY & stream;
    _cumulativeYlag & stream;
    timeStepMaxDensity & stream;
    (*pathogenesisModel) & stream;
}
void WHFalciparum::checkpoint (ostream& stream) {
    WHInterface::checkpoint( stream );
    _innateImmSurvFact & stream;
    _cumulativeh & stream;
    _cumulativeY & stream;
    _cumulativeYlag & stream;
    timeStepMaxDensity & stream;
    (*pathogenesisModel) & stream;
}

}
}