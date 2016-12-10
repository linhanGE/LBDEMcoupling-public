/*
 * This file is part of the LBDEMcoupling software.
 *
 * LBDEMcoupling is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright 2014 Johannes Kepler University Linz
 *
 * Author: Philippe Seil (philippe.seil@jku.at)
 */

#ifndef IB_COMPOSITE_DYNAMICS_HH_LBDEM
#define IB_COMPOSITE_DYNAMICS_HH_LBDEM

#include "ibDef.h"

namespace plb {
  template<typename T, template<typename U> class Descriptor>
  int IBcompositeDynamics<T,Descriptor>::id =
    meta::registerGeneralDynamics<T,Descriptor, IBcompositeDynamics<T,Descriptor> >("IBcomposite");

  template<typename T, template<typename U> class Descriptor>
  Array<pluint,Descriptor<T>::q> IBcompositeDynamics<T,Descriptor>::iOpposite =
    Array<pluint,Descriptor<T>::q>();

  template<typename T, template<typename U> class Descriptor>
  Array<T,Descriptor<T>::q> IBcompositeDynamics<T,Descriptor>::fEqSolid =
    Array<T,Descriptor<T>::q>();

  template<typename T, template<typename U> class Descriptor>
  Array<T,Descriptor<T>::q> IBcompositeDynamics<T,Descriptor>::fPre =
    Array<T,Descriptor<T>::q>();

  template<typename T, template<typename U> class Descriptor>
  IBcompositeDynamics<T,Descriptor>::IBcompositeDynamics(Dynamics<T,Descriptor>* baseDynamics_,
							 bool automaticPrepareCollision_)
    : CompositeDynamics<T,Descriptor>(baseDynamics_,automaticPrepareCollision_), 
    IBdynamicsParticleData<T,Descriptor>()
  { 
    static bool init_iOpposite = false;
    if(!init_iOpposite){
      plint const shift = Descriptor<T>::q/2;
      iOpposite[0] = 0;
      for(plint iPop=1;iPop<Descriptor<T>::q;iPop++){
        iOpposite[iPop] = iPop <= shift ? iPop + shift : iPop - shift;
      }      
      init_iOpposite = true;
    }
  }
  
  template<typename T, template<typename U> class Descriptor>
  IBcompositeDynamics<T,Descriptor>::IBcompositeDynamics(HierarchicUnserializer &unserializer)
    : CompositeDynamics<T,Descriptor>(0,false),
    IBdynamicsParticleData<T,Descriptor>()
  {
    // pcout << "entering serialize constructor" << std::endl;
    unserialize(unserializer);
  }

  template<typename T, template<typename U> class Descriptor>
  IBcompositeDynamics<T,Descriptor>::IBcompositeDynamics(const IBcompositeDynamics &orig)
    : CompositeDynamics<T,Descriptor>(orig),
    IBdynamicsParticleData<T,Descriptor>(orig)
  { }
  
  template<typename T, template<typename U> class Descriptor>
  IBcompositeDynamics<T,Descriptor>::~IBcompositeDynamics() {}
  
  template<typename T, template<typename U> class Descriptor>
  IBcompositeDynamics<T,Descriptor>* IBcompositeDynamics<T,Descriptor>::clone() const {
    return new IBcompositeDynamics<T,Descriptor>(*this);
  }
  
  template<typename T, template<typename U> class Descriptor>
  int IBcompositeDynamics<T,Descriptor>::getId() const
  {
    return id;
  }
  
  template<typename T, template<typename U> class Descriptor>
  void IBcompositeDynamics<T,Descriptor>::serialize(HierarchicSerializer &serializer) const
  {

    this->particleData.serialize(serializer);
    CompositeDynamics<T,Descriptor>::serialize(serializer);
  }

  template<typename T, template<typename U> class Descriptor>
  void IBcompositeDynamics<T,Descriptor>::unserialize(HierarchicUnserializer &unserializer)
  {
    PLB_PRECONDITION( unserializer.getId() == this->getId() );

    this->particleData.unserialize(unserializer);
    CompositeDynamics<T,Descriptor>::unserialize(unserializer);
  }
  
  template<typename T, template<typename U> class Descriptor>
  void IBcompositeDynamics<T,Descriptor>::prepareCollision(Cell<T,Descriptor>& cell)
  {

    if(this->particleData.solidFraction > SOLFRAC_MIN)
      fPre = cell.getRawPopulations();

  }

  template<typename T, template<typename U> class Descriptor>
  void IBcompositeDynamics<T,Descriptor>::defineVelocity(Cell<T,Descriptor>& cell, 
                                                         Array<T,Descriptor<T>::d> const& u)
  {
    Array<T,Descriptor<T>::q> fEq;
    T const rhoBar = 1.;
    T const uSqr = VectorTemplateImpl<T,Descriptor<T>::d>::normSqr(u); 
    CompositeDynamics<T,Descriptor>::getBaseDynamics().computeEquilibria(fEq,0.,u,uSqr);
    for(plint i=0;i<Descriptor<T>::q;i++)
      cell[i] = fEq[i];
  }
  
  /// Implementation of the collision step
  template<typename T, template<typename U> class Descriptor>
  void IBcompositeDynamics<T,Descriptor>::collide(Cell<T,Descriptor>& cell,
                                                  BlockStatistics& statistics)
  {

    prepareCollision(cell);

    this->particleData.hydrodynamicForce.resetToZero();

    if(this->particleData.solidFraction <= SOLFRAC_MAX)
      CompositeDynamics<T,Descriptor>::getBaseDynamics().collide(cell,statistics);
    
    if(this->particleData.solidFraction < SOLFRAC_MIN)
      return;

    T const rhoBar = momentTemplates<T,Descriptor>::get_rhoBar(cell);
    Array<T,Descriptor<T>::d> uPart = this->particleData.uPart*(1.+rhoBar);
    T const uPartSqr = VectorTemplateImpl<T,Descriptor<T>::d>::normSqr(uPart); 
    CompositeDynamics<T,Descriptor>::getBaseDynamics().computeEquilibria(fEqSolid,rhoBar,uPart,uPartSqr);

    if(this->particleData.solidFraction > SOLFRAC_MAX){
      cell[0] = fPre[0];

      for(plint iPop=1;iPop<Descriptor<T>::q;iPop++){
        T coll = 0.5*(fPre[iOpposite[iPop]] - fEqSolid[iOpposite[iPop]] + fEqSolid[iPop] - fPre[iPop]);
        
        cell[iPop] = fPre[iPop] + coll;

        for(plint iDim=0;iDim<Descriptor<T>::d;iDim++)
          this->particleData.hydrodynamicForce[iDim] -= Descriptor<T>::c[iPop][iDim]*coll;
      }
    } else {
      T const ooo = 1./CompositeDynamics<T,Descriptor>::getBaseDynamics().getOmega() - 0.5;

      #ifdef LBDEM_USE_WEIGHING
      T const B = this->particleData.solidFraction*ooo/((1.- this->particleData.solidFraction) + ooo);
      #else
      T const B = this->particleData.solidFraction;
      #endif

      T const oneMinB = 1. - B;

      cell[0] = fPre[0] + oneMinB*(cell[0] - fPre[0]);

      for(plint iPop=1;iPop<Descriptor<T>::q;iPop++){
        T coll = -B*0.5*(fPre[iOpposite[iPop]] - fEqSolid[iOpposite[iPop]] + fEqSolid[iPop] - fPre[iPop]);

        cell[iPop] = fPre[iPop] + oneMinB*(cell[iPop] - fPre[iPop]) - coll;

        for(plint iDim=0;iDim<Descriptor<T>::d;iDim++)
          this->particleData.hydrodynamicForce[iDim] += Descriptor<T>::c[iPop][iDim]*coll;
      }
    }
    
  }

}; /* namespace plb */

#endif /* IB_COMPOSITE_DYNAMICS_HH_LBDEM */
