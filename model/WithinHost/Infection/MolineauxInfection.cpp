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

#include "WithinHost/Infection/MolineauxInfection.h"
#include "WithinHost/CommonWithinHost.h"
#include "util/random.h"
#include "util/errors.h"
#include "util/CommandLine.h"
#include "util/ModelOptions.h"
#include "util/checkpoint_containers.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <cmath>
#include <boost/static_assert.hpp>


namespace OM {
namespace WithinHost {

using namespace OM::util;

double MolineauxInfection::mean_shape_first_local_max = std::numeric_limits<double>::signaling_NaN();
double MolineauxInfection::sd_scale_first_local_max  = std::numeric_limits<double>::signaling_NaN();
double MolineauxInfection::mean_shape_diff_pos_days  = std::numeric_limits<double>::signaling_NaN();
double MolineauxInfection::sd_scale_diff_pos_days  = std::numeric_limits<double>::signaling_NaN();

bool MolineauxInfection::first_local_maximum_gamma = false;
bool MolineauxInfection::mean_duration_gamma = false;
bool MolineauxInfection::multi_factor_gamma  = false;
bool MolineauxInfection::pairwise_PStar_sample = false;

double MolineauxInfection::qPow[v];

/** @brief The static variables (double)
 *
 * sProb: fraction of parasites switching among variants per two-day cycle
 * q: Parameter of the geometric distribution of switching probabilities
 * k_c,k_m: constants allowing calculation of Pstar_c and Pstar_m from host-specific data
 * Pstar_v: critical density of a variant, common to all variants
 * kappa_c, kappa_m, kappa_v: Stiffness parameters for saturation of immune responses
 * C: Maximum daily antigenic stimulus, per mul, of the acquired variant-transcending immune response
 * sigma, rho: decay parameters, per day, of the acquired variant-specific and variant-transcending immune responses
 * beta: Minimum value of the probability that a parasite escape control by the acquired and variant-transcending immune response
 * mu_m, sigma_m: Mean and standard deviation to use for the normal distribution setting the variant specific multiplication factor.
 */
//@{
const double sigma = 0.02;
const double sigma_decay=exp(-2.0*sigma);
const double rho=0.0;
const double beta=0.01;
const double sProb=0.02;
const double q=0.3;

const double mu_m=16.0;
const double sigma_m=10.4;
const double shape_m=2.4;
const double scale_m=6.8;

const double k_c=0.2;
const double k_m=0.04;
const double Pstar_v=30.0;
const int kappa_c=3;
const int kappa_m=1;
const int kappa_v=3;
const double C=1.0;

/* Case-specific parameters.
 * 
 * For each of the 35 Malaria Therapy patients, this is:
 *  0) the duration of the infection as last day with positive density minus first positive
 *  1) the parasite density in parasites/microlitre at the first local maximum
 * 
 * Indexing is 2*patient + param where 0 <= patient < 35 and 0 <= param < 2.
 */
const double caseSpecificData[] = {
        216, 18600, // G131
        198, 13080, // G140
        206, 45720, // G141
        366, 23760, // G142
        230, 60840, // G143
        172, 6000, // G146
        100, 2340, // G147
        236, 31440, // G159
        236, 453600, // G161
        120, 4240, // G173
        176, 195840, // G174
        178, 60120, // G178
        36, 8720, // G184
        44, 8000, // G193
        242, 395280, // G200
        70, 28320, // G210
        292, 200160, // G212
        248, 59320, // G217
        98, 66480, // G23
        176, 61200, // G230
        234, 169920, // G240
        226, 46800, // G264
        270, 19260, // G265
        278, 86040, // G279
        212, 110160, // G290
        264, 43200, // G385
        364, 133920, // G407
        184, 222480, // G408
        160, 21420, // G414
        220, 74160, // G416
        132, 210960, // G423
        176, 89280, // G439
        208, 105840, // G445
        330, 21600, // G457
        404, 156240 // G48
};
//@}

CommonInfection* createMolineauxInfection (uint32_t protID) {
    return new MolineauxInfection (protID);
}

CommonInfection* checkpointedMolineauxInfection (istream& stream) {
    return new MolineauxInfection (stream);
}

void MolineauxInfection::init( const Parameters& parameters ){
    CommonWithinHost::createInfection = &createMolineauxInfection;
    CommonWithinHost::checkpointedInfection = &checkpointedMolineauxInfection;
    
    if(util::ModelOptions::option (util::PARASITE_REPLICATION_GAMMA)) {
	multi_factor_gamma = true;
    } else {
	multi_factor_gamma = false;
    }
    
    pairwise_PStar_sample = util::ModelOptions::option( util::MOLINEAUX_PAIRWISE_SAMPLE );
    if( !pairwise_PStar_sample ){
        mean_shape_first_local_max = parameters[Parameters::MEAN_LOCAL_MAX_DENSITY];
        sd_scale_first_local_max = parameters[Parameters::SD_LOCAL_MAX_DENSITY];
        
        mean_shape_diff_pos_days = parameters[Parameters::MEAN_DIFF_POS_DAYS];
        sd_scale_diff_pos_days = parameters[Parameters::SD_DIFF_POS_DAYS];
        
        
        // with gamma distribution shape and scale parameters has to be recalculated 
        if (util::ModelOptions::option (util::FIRST_LOCAL_MAXIMUM_GAMMA)) {
            first_local_maximum_gamma = true;
            mean_shape_first_local_max = pow(mean_shape_first_local_max,2)/pow(sd_scale_first_local_max,2);
            sd_scale_first_local_max = pow(sd_scale_first_local_max,2)/mean_shape_first_local_max;
        } else {
            first_local_maximum_gamma = false;
        }
        
        // with gamma distribution shape and scale parameters has to be recalculated
        if(util::ModelOptions::option (util::MEAN_DURATION_GAMMA)) {
            mean_duration_gamma = true;
            mean_shape_diff_pos_days = pow(mean_shape_diff_pos_days,2)/pow(sd_scale_diff_pos_days,2);
            sd_scale_diff_pos_days = pow(sd_scale_diff_pos_days,2)/mean_shape_diff_pos_days;
        } else {
            mean_duration_gamma = false;
        }
    }

   for(int i=0;i<50;i++)
   {
       qPow[i] = pow(q,(double)(i+1));
   }
}

MolineauxInfection::MolineauxInfection(uint32_t protID):
        CommonInfection(protID)
{
    for (size_t i=0;i<v; i++)
    {
        m[i] = 0.0;
	// Molineaux paper, equation 11
	if( multi_factor_gamma ) {
	  while (m[i]<1.0) {    
	    m[i]=static_cast<float>(random::gamma(shape_m,scale_m));
	  }
	} else {
	  while (m[i]<1.0) {    
	    m[i]=static_cast<float>(random::gauss(mu_m, sigma_m));
	  }
	}
    }

    for (size_t tau=0; tau<taus;tau++)
    {
        laggedPc[tau] = 0.0;
    }

    // the initial density is set to 0.1... The first chosen variant is the variant 1
    variants.resize(1);
    variants[0].P = 0.1f;
    variantTranscendingSummation = 0.0;
    
    if( pairwise_PStar_sample ){
        int patient = util::random::uniform( 35 );
        Pstar_c = k_c * caseSpecificData[2 * patient + 1];
        Pstar_m = k_m * caseSpecificData[2 * patient + 0];
    }else{
        // sampling first local maximum
        // for gamma distribution shape and scale parameters are > 0 so if parameter == 0 mean that gauss distibution is choosen
        if( first_local_maximum_gamma ) {
            Pstar_c = static_cast<float>(k_c*pow(random::gamma(mean_shape_first_local_max,sd_scale_first_local_max),10.0));
        } else {
            Pstar_c = static_cast<float>(k_c*pow(random::gauss(mean_shape_first_local_max,sd_scale_first_local_max),10.0));
        }
        
        // sampling duration
        // for gamma distribution shape and scale parameters are > 0 so if parameter == 0 mean that gauss distibution is choosen
        if( mean_duration_gamma ) {
            Pstar_m = static_cast<float>(k_m*pow(random::gamma(mean_shape_diff_pos_days,sd_scale_diff_pos_days),10.0));
        } else {
            Pstar_m = static_cast<float>(k_m*pow(random::gauss(mean_shape_diff_pos_days,sd_scale_diff_pos_days),10.0));
        }
    }
}

MolineauxInfection::Variant::Variant () :
        growthRate(0.0), P(0.0), variantSpecificSummation(0.0), initP(0.0)
{
    for (size_t tau=0; tau<taus; tau++)
    {
        laggedP[tau] =  0.0;
    }
}

void MolineauxInfection::Variant::updateGrowthRateMultiplier( double pd, double immune_response_escape ){
    // Molineaux paper equation 1
    // newPi: Variant density at t = t + 2
    // new variant density  = (the amount of this variant's parasites
    // which will not switch to another variant + the ones from other
    // variants switching to this variant) * this variant multiplication
    // factor * the probability that the parasites escape control by immune response.
    double newPi = ( (1.0-sProb) * P + sProb*pd )*immune_response_escape;

    // Molineaux paper equation 2
    if (newPi<1.0e-5)
    {
	newPi = 0.0;
    }

    // if P == 0 then that means this variant wasn't expressed yet
    // or is extinct. If this variant is emerging in (t+2) the new variant
    // density is stored in the initP array,  so that we are able to add
    // the survival factor's effect to the emerging variant's density.
    if (P==0) {
	initP = static_cast<float>(newPi);
	growthRate = 0.0;
    }
    else {
	initP = 0.0;
	growthRate = static_cast<float>(sqrt(newPi/P));
    }
}

void MolineauxInfection::updateGrowthRateMultiplier(int ageDays) {
    // The immune responses are represented by the variables
    // Sc (probability that a parasite escapes control by innate and variant-transcending immune response)
    // Sm (                        "                      acquired and variant-transcending immune response)
    // S[i] (                      "                      acquired and variant-specific immune response)
    // We're using multiplication instead of power for speed here. Confirm exponent:
    BOOST_STATIC_ASSERT( kappa_c == 3 );
    double base = _density/Pstar_c;
    double Sc = 1.0 / (1.0 + base*base*base);

    //double Sm = ((1-beta)/(1+pow(getVariantTranscendingSummation()/Pstar_m, kappa_m)))+beta
    //optimization: Since kappa_m = 1, we don't use pow.
    double Sm = ((1.0-beta)/(1.0+(getVariantTranscendingSummation(ageDays)/Pstar_m)))+beta;
    double S[v];

    double sigma_Qi_Si=0.0;
    
    for (size_t i=0; i<v; i++)
    {
        S[i] = 1.0;
	if( i < variants.size() ){
	    // As above, confirm exponent:
	    BOOST_STATIC_ASSERT( kappa_v == 3 );
	    double base = variants[i].getVariantSpecificSummation(ageDays) / Pstar_v;
	    S[i] = 1.0 / (1.0 + base*base*base);
	}
        sigma_Qi_Si+= qPow[i]*S[i];
    }

    for (size_t i=0;i<v;i++)
    {
        // Molineaux paper equation 4
        // p_i: variant selection probability
        double p_i;
        if ( S[i]<0.1 ) {
            p_i = 0.0;
        } else {
            p_i = qPow[i]*S[i]/sigma_Qi_Si;
        }
	
	if( i < variants.size() ){
            double immune_response_escape = m[i]*S[i]*Sc*Sm;
	    variants[i].updateGrowthRateMultiplier( p_i*_density, immune_response_escape );
	} else {
	    // Molineaux paper equation 1
	    // newPi: Variant density at t = t + 2
	    // new variant density  = (the amount of this variant's parasites
	    // which will not switch to another variant + the ones from other
	    // variants switching to this variant) * this variant multiplication
	    // factor * the probability that the parasites escape control by immune response.
	    double newPi = ( sProb*p_i*_density )*m[i]*S[i]*Sc*Sm;

	    // Molineaux paper equation 2
	    if (newPi<1.0e-5)
	    {
		newPi = 0.0;
	    } else {
		// variant wasn't expressed yet
		
		variants.resize( i+1 );
		variants[i].initP = static_cast<float>(newPi);
	    }
	}
    }
}

double MolineauxInfection::Variant::updateDensity (double survivalFactor, int ageDays) {
    // growthRate:
    // p(t+1) = p(t) * sqrt(p(t+2)/p(t))
    // p(t+2) = p(t+1) * sqrt(p(t+2)/p(t))
    // p(t+2) = p(t) * sqrt(p(t+2)/p(t))^2...
    P *= growthRate;

    // survivalFactor: effects of drugs, immunity and vaccines
    P = static_cast<float>(P * survivalFactor);
    initP = static_cast<float>(initP * survivalFactor);

    // if t+2: The new variant is now expressed. For already extinct
    // variants this doesn't matter, since initP = 0 for those variants.
    if (P==0 && mod_nn(ageDays, 2)==0)
    {
        P = initP;
    }

    // Molineaux paper equation 3
    // The variant is extinct when variant's density < 1.0e-5
    if (P<1.0e-5)
    {
        P = 0.0;
    }
    return P;
}

bool MolineauxInfection::updateDensity(double survivalFactor, int ageDays) {
    if (ageDays == 0){
        _density = variants[0].P;
    }else{
        double newDensity = 0.0;
        for (size_t i=0;i<variants.size();i++){
            newDensity += variants[i].updateDensity( survivalFactor, ageDays );
        }
        _density = newDensity;
    }

    // Note: normally this is multiplied by the 
    _cumulativeExposureJ += _density;

    if( _density <= 1.0e-5 ){
        return true;    // infection goes extinct
    }
    
    // if the infection isn't extinct and t = t+2
    // then the growthRateMultiplier is adapted for t+3 and t+4
    if( mod_nn(ageDays, 2) == 0 ){
        updateGrowthRateMultiplier(ageDays);
    }
    return false;
}

double MolineauxInfection::Variant::getVariantSpecificSummation(int ageDays) {
    //The effective exposure is computed by adding in the 8-day lagged parasite density (i.e. 4 time steps)
    //and decaying the previous value for the effective exposure with decay parameter 2*sigma (the 2 arises because
    //the time steps are two days and the unit of sigma is per day. (reasoning: rearrangment of Molineaux paper equation 6)

    //Molineaux paper equation 6
    size_t index = mod_nn(ageDays, 8)/2;	// 8 days ago has same index as today
    //note: sigma_decay = exp(-2*sigma)
    variantSpecificSummation = static_cast<float>((variantSpecificSummation * sigma_decay)+laggedP[index]);
    laggedP[index] = P;

    return variantSpecificSummation;
}

double MolineauxInfection::getVariantTranscendingSummation(int ageDays) {

    //Molineaux paper equation 5
    size_t index = mod_nn(ageDays, 8)/2;	// 8 days ago has same index as today
    //Note: rho is zero, so the decay here is unnecessary:
    variantTranscendingSummation = (variantTranscendingSummation /* * exp(-2.0*rho) */)+laggedPc[index];

    //Molineaux paper equation 8
    //We could use min here, but it seems that min has problems with static const double C
    laggedPc[index] = static_cast<float>(_density < C ? _density:C);

    return variantTranscendingSummation;
}

MolineauxInfection::MolineauxInfection (istream& stream) :
        CommonInfection(stream)
{
    variantTranscendingSummation & stream;
    for (size_t i=0;i<v;i++) {
        m[i] & stream;
    }
    variants & stream;
    for (size_t j=0;j<taus;j++)
    {
        laggedPc[j] & stream;
    }
    Pstar_c & stream;
    Pstar_m & stream;
}

void MolineauxInfection::checkpoint (ostream& stream) {
    CommonInfection::checkpoint (stream);

    variantTranscendingSummation & stream;
    for (size_t i=0;i<v;i++) {
        m[i] & stream;
    }
    variants & stream;
    for (size_t j=0;j<taus;j++)
    {
        laggedPc[j] & stream;
    }
    Pstar_c & stream;
    Pstar_m & stream;
}

void MolineauxInfection::Variant::operator& (istream& stream) {
    bool nonZero;
    nonZero & stream;
    if( nonZero ){
	growthRate & stream;
	P & stream;
	variantSpecificSummation & stream;
	initP & stream;
	for(size_t i = 0; i < taus; ++i)
	    laggedP[i] & stream;
    }
    else
    {
	growthRate = 0.0;
	P = 0.0;
	variantSpecificSummation = 0.0;
	initP = 0.0;
	for(size_t i = 0; i < taus; ++i)
	    laggedP[i] = 0.0;
    }
}

void MolineauxInfection::Variant::operator& (ostream& stream) {
    bool nonZero =
	    growthRate != 0.0 ||
	    P != 0.0 ||
	    variantSpecificSummation != 0.0 ||
	    initP != 0.0;

    nonZero & stream;
    if( nonZero ){
	growthRate & stream;
	P & stream;
	variantSpecificSummation & stream;
	initP & stream;
	for(size_t i = 0; i < taus; ++i)
	    laggedP[i] & stream;
    }
}

}
}
