/* SPDX-FileCopyrightText: 2009 Ruben Smits
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later */

/** \file
 * \ingroup intern_itasc
 */

#ifndef OBJECT_HPP_
#define OBJECT_HPP_

#include "Cache.hpp"
#include "kdl/frames.hpp"
#include <string>

namespace iTaSC{

class WorldObject;

class Object {
public:
    enum ObjectType {Controlled, UnControlled};
	static WorldObject world;

private:
    ObjectType m_type;
protected:
	Cache *m_cache;
	KDL::Frame m_internalPose;
	bool m_updated;
    virtual void updateJacobian()=0;
public:
    Object(ObjectType _type):m_type(_type), m_cache(NULL), m_internalPose(F_identity), m_updated(false) {};
    virtual ~Object(){};

	virtual int addEndEffector(const std::string&  /*name*/){return 0;};
	virtual bool finalize(){return true;};
	virtual const KDL::Frame& getPose(const unsigned int end_effector=0){
		(void)end_effector;
		return m_internalPose;
	};
    virtual ObjectType getType(){return m_type;};
    virtual unsigned int getNrOfCoordinates(){return 0;};
    virtual void updateKinematics(const Timestamp& /*timestamp*/)=0;
    virtual void pushCache(const Timestamp& /*timestamp*/)=0;
	virtual void initCache(Cache * /*cache*/) = 0;
	bool updated() {return m_updated;};
	void updated(bool val) {m_updated=val;};
};

}
#endif /* OBJECT_HPP_ */
