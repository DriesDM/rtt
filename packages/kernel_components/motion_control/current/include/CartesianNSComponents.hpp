/***************************************************************************
  tag: Peter Soetens  Mon Jan 19 14:11:21 CET 2004  CartesianNSComponents.hpp 

                        CartesianNSComponents.hpp -  description
                           -------------------
    begin                : Mon January 19 2004
    copyright            : (C) 2004 Peter Soetens
    email                : peter.soetens@mech.kuleuven.ac.be
 
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
 
 
#ifndef CARTESIAN_COMPONENTS_HPP
#define CARTESIAN_COMPONENTS_HPP

#include <geometry/frames.h>
#include <geometry/frames_io.h>
#include <geometry/trajectory.h>
#include <geometry/trajectory_segment.h>
#include <geometry/path_line.h>
#include <geometry/velocityprofile_trap.h>
#include <geometry/rotational_interpolation_sa.h>
#include <geometry/MotionProperties.hpp>
#include <corelib/HeartBeatGenerator.hpp>
#include <kindyn/KinematicsComponent.hpp>
#include <kindyn/KinematicsFactory.hpp>
#include <corelib/EventInterfaces.hpp>
#include <kernel_components/Simulator.hpp>

#include <pkgconf/system.h>
#ifdef OROPKG_EXECUTION_PROGRAM_PARSER
#include "execution/TemplateDataSourceFactory.hpp"
#include "execution/TemplateCommandFactory.hpp"
#endif


/**
 * @file CartesianNSComponents.hpp
 *
 * This file contains components for
 * use in cartesian path planning. Only Nameserved
 * DataObjects are used.
 */
    
namespace ORO_ControlKernel
{
    using namespace ORO_Geometry;
    using namespace ORO_CoreLib;
    using namespace ORO_KinDyn;
#ifdef OROPKG_EXECUTION_PROGRAM_PARSER
    using namespace ORO_Execution;
#endif

    /**
     * @brief The Nameserved Command DataObjects.
     */
    struct CartNSCommands
        : public ServedTypes< Trajectory*, Frame>, public UnServedType<>
    {
        CartNSCommands()
        {
            // The numbers 0,1 is the type number as in
            // ServedTypes< Type0, Type1, Type2,... >
            this->insert( make_pair(0, "Trajectory"));
            this->insert( make_pair(1, "TaskFrame"));
            this->insert( make_pair(1, "ToolFrame"));
        }
    };
 
    /**
     * The SetPoint is expressed in the robot base frame.
     */
    struct CartNSSetPoints
        : public ServedTypes<Frame>, public UnServedType< >
    {
        CartNSSetPoints()
        {
            this->insert( make_pair(0, "EndEffectorFrame"));
        }
    };

    /**
     * A Simple Cartesian Generator.
     */
    template <class Base>
    class CartesianGenerator 
        : public Base
    {
    public:
        /**
         * Necessary typedefs.
         */
        typedef typename Base::SetPointType SetPointType;
        typedef typename Base::InputType InputType;
        typedef typename Base::CommandType CommandType;
        typedef typename Base::ModelType ModelType;
            
        /**
         * Constructor.
         */
        CartesianGenerator() 
            : Base("CartesianGenerator"),
              homepos(Frame::Identity()),
              timestamp(0), _time(0), cur_tr(0),
              task_frame(Frame::Identity()),
              tool_mp_frame(Frame::Identity()),
              end_pos("End Position","One of many variables which can be reported.")
        {}

        /**
         * @see KernelReportingExtension.hpp class ReportingComponent
         */
        virtual void exportReports(PropertyBag& bag)
        {
            bag.add(&end_pos);
        }

        virtual bool componentStartup()
        {
            // startup the component, acquire all the data objects,
            // put sane defaults in the setpoint dataobject.
            if ( !Base::Command::dObj()->Get("Trajectory", traj_DObj) ||
                 !Base::Command::dObj()->Get("ToolFrame", tool_f_DObj) ||
                 !Base::Command::dObj()->Get("TaskFrame", task_f_DObj) ||
                 !Base::Model::dObj()->Get("EndEffPosition", mp_base_f_DObj) ||
                 !Base::SetPoint::dObj()->Get("EndEffectorFrame", end_f_DObj) )
                return false;

            // stay as is.
            mp_base_f_DObj->Get( mp_base_frame );
            end_f_DObj->Set( mp_base_frame );

            // record the startup time. 
            timestamp = HeartBeatGenerator::Instance()->ticksGet();
            return true;
        }            
                
        /**
         * @see KernelInterfaces.hpp class ModuleControlInterface
         */
        virtual void pull()
        {
            // Always read the current position
            mp_base_f_DObj->Get( mp_base_frame );
            _time = HeartBeatGenerator::Instance()->secondsSince(timestamp);
        }
            
        /**
         * @see KernelInterfaces.hpp class ModuleControlInterface
         */
        virtual void calculate() 
        {
            if ( cur_tr )
                {
                    end_pos = task_frame * cur_tr->Pos(_time) * tool_mp_frame.Inverse();
                }
        }
            
        /**
         * @see KernelInterfaces.hpp class ModuleControlInterface
         */
        virtual void push()      
        {
            end_f_DObj->Set( end_pos );
        }
            
        bool trajectoryDone()
        {
            if (cur_tr)
                return _time > cur_tr->Duration();
            return true;
        }
        
        Frame position()
        {
            return mp_base_frame;
        }

        double time()
        {
            return _time;
        }

        /**
         * Read the trajectory stored in the command and 
         * use it for interpolation.
         */
        void loadTrajectory()
        {
            if ( kernel()->isRunning() && trajectoryDone() )
                {
                    /**
                     * First, be sure the given traj is not zero.
                     * Then, check if we are interpolating a traj or not.
                     * In wichever case we are, the new traj should start within
                     * a margin from the current generated position.
                     */
                    if (  traj_DObj->Get() != 0 && 
                          (( cur_tr !=0 &&
                             Equal( task_frame * cur_tr->Pos(_time) * tool_mp_frame.Inverse(), 
                                    task_f_DObj->Get() * traj_DObj->Get()->Pos(0) * tool_mp_frame.Inverse(),0.01) )
                           ||
                           ( cur_tr == 0 &&
                             Equal( mp_base_frame,
                                    task_f_DObj->Get() * traj_DObj->Get()->Pos(0) * tool_mp_frame.Inverse(),0.01))))
                        {
                            cout << "Load Trajectory"<<endl;
                            // get new trajectory
                            traj_DObj->Get(cur_tr);
                            task_f_DObj->Get(task_frame);
                            tool_f_DObj->Get(tool_mp_frame);
                            timestamp = HeartBeatGenerator::Instance()->ticksGet();
                            _time = 0;
                        }
                    else cout << "Trajectory not loaded"<<endl;
                }
            cout << "exit loadTrajectory()"<<endl;
        }

        /**
         * Home to a predefined position.
         */
        void home()
        {
            // XXX insert proper delete code.
            if ( kernel()->isRunning() && trajectoryDone() )
                {
                    cout <<"Home : from "<< mp_base_frame <<" to "<<endl<< homepos <<endl;
                    cur_tr = new Trajectory_Segment( new Path_Line(mp_base_frame, homepos,
                                                                   new RotationalInterpolation_SingleAxis(),1.0 ),
                                                     new VelocityProfile_Trap(1,10),10.0);
                    task_frame = Frame::Identity();
                    tool_mp_frame = Frame::Identity();
                    timestamp = HeartBeatGenerator::Instance()->ticksGet();
                    _time = 0;
                }
        }

        bool isHomed()
        {
            return trajectoryDone();
        }

        bool isTrajectoryLoaded()
        {
            return cur_tr != 0;
        }

        void setHomeFrame( Frame _homepos )
        {
            homepos = _homepos;
        }

#ifdef OROPKG_EXECUTION_PROGRAM_PARSER

        DataSourceFactory* createDataSourceFactory()
        {
            TemplateDataSourceFactory< CartesianGenerator<Base> >* ret =
                newDataSourceFactory( this );
            ret->add( "position", 
                      data( &CartesianGenerator<Base>::position, "The current position "
                            "of the robot." ) );
            ret->add( "time",
                      data( &CartesianGenerator<Base>::time, 
                            "The current time in the movement "
                            ) );
            ret->add( "trajectoryDone",
                      data( &CartesianGenerator<Base>::trajectoryDone,
                            "The state of the current trajectory "
                            ) ); 
            return ret;
        }

        template< class T >
        bool true_gen( T t ) { return true; }

        CommandFactoryInterface* createCommandFactory()
        {
            TemplateCommandFactory< CartesianGenerator<Base> >* ret =
                newCommandFactory( this );
            ret->add( "home", 
                      command( &CartesianGenerator<Base>::home,
                               &CartesianGenerator<Base>::isHomed,
                               "Move the robot to its home position" ) );
            ret->add( "setHomeFrame", 
                      command( &CartesianGenerator<Base>::setHomeFrame,
                               &CartesianGenerator<Base>::true_gen,
                            "The home position of the robot.", "Frame", "Homing End Frame" ) );
            ret->add( "loadTrajectory",
                      command( &CartesianGenerator<Base>::loadTrajectory,
                               &CartesianGenerator<Base>::isTrajectoryLoaded,
                               "Load a new trajectory." ) );
            return ret;
        }
#endif
    protected:
        Frame homepos;
        HeartBeatGenerator::ticks timestamp;
        Seconds      _time;

        Trajectory*  cur_tr;
        typename Base::Command::DataObject<Trajectory*>::type* traj_DObj;

        Frame task_frame;
        typename Base::Command::DataObject<Frame>::type* task_f_DObj;

        Frame tool_mp_frame;
        typename Base::Command::DataObject<Frame>::type* tool_f_DObj;

        Property<Frame> end_pos;
        typename Base::SetPoint::DataObject<Frame>::type* end_f_DObj;

        Frame mp_base_frame;
        typename Base::Model::DataObject<Frame>::type* mp_base_f_DObj;
    };
    
    /**
     * The Estimator only estimates the cartesian coordinates.
     */
    struct CartNSModel
        : public ServedTypes<Frame>, public UnServedType< >
    {
        CartNSModel() {
            this->insert(make_pair(0, "EndEffPosition"));
        }
    };

    /**
     * A Cartesian Estimator
     */
    template <class Base>
    class CartesianEstimator
        : public Base
    {
    public:
        /**
         * Necessary typedefs.
         */
        typedef typename Base::InputType InputType;
        typedef typename Base::ModelType ModelType;
            
        CartesianEstimator() 
            : Base("CartesianEstimator"),
              kineName("Kinematics","The name of the KinematicsStub to use","Kuka361"), kine(0),kineComp(0)
        {
            kine = KinematicsFactory::create( kineName );
            if (kine)
                kineComp = new KinematicsComponent( kine );
        }

        virtual ~CartesianEstimator()
        {
            if (kine) 
                {
                    delete kine;
                    delete kineComp;
                }
        }

        virtual bool componentStartup()
        {
            // Get the JointPositions DataObject and the 
            if ( !Base::Input::dObj()->Get( "JointPositions", jpos_DObj ) ||
                 !Base::Model::dObj()->Get( "EndEffPosition", endframe_DObj ) )
                return false;
            pull();
            if ( !kineComp->positionForward( q6, mp_base_frame) )
                return false;
            push();
            return true;
        }
            
        /**
         * @see KernelInterfaces.hpp class ModuleControlInterface
         */
        virtual void pull()      
        {
            jpos_DObj->Get( q6 );
        }
            
        /**
         * @see KernelInterfaces.hpp class ModuleControlInterface
         */
        virtual void calculate() 
        {
            if ( !kineComp->positionForward( q6, mp_base_frame) )
                {
                    EventOperationInterface* es = EventOperationInterface::nameserver.getObjectByName("SingularityDetected");
                    if ( es != 0 )
                        es->fire();
                }
        }
            
        /**
         * @see KernelInterfaces.hpp class ModuleControlInterface
         */
        virtual void push()      
        {
            endframe_DObj->Set( mp_base_frame );
        }
            
        /**
         * @see KernelReportingExtension.hpp class ReportingComponent
         */
        virtual void exportReports(PropertyBag& bag)
        {
        }

        /**
         * @see KernelPropertyExtension.hpp class PropertyComponentInterface
         */
        virtual bool updateProperties( const PropertyBag& bag)
        {
            if ( composeProperty(bag, kineName ) )
                {
                    kine = KinematicsFactory::create( kineName );
                    if (kine)
                        {
                            kineComp = new KinematicsComponent( kine );
                            return true;
                        }
                }
            return false;
        }

    protected:
        Property<string> kineName;
        KinematicsInterface* kine;
        KinematicsComponent* kineComp;

        Double6D q6;
        typename Base::Input::DataObject<Double6D>::type* jpos_DObj;

        Frame mp_base_frame;
        typename Base::Model::DataObject<Frame>::type* endframe_DObj;
    };

    /**
     * Calculated joint velocities.
     */
    struct CartNSDriveOutputs
        : public ServedTypes<Double6D>, public UnServedType< >
    {
        CartNSDriveOutputs()
        {
            this->insert(make_pair(0, "JointVelocities"));
        }
    };

    /**
     * A Cartesian Controller
     */
    template <class Base>
    class CartesianController
        : public Base
    {
    public:
        /**
         * Necessary typedefs.
         */
        typedef typename Base::SetPointType SetPointType;
        typedef typename Base::InputType InputType;
        typedef typename Base::ModelType ModelType;
        typedef typename Base::OutputType OutputType;
            
        CartesianController(KinematicsInterface* k) 
            :  Base("CartesianController"),gain("Gain","The error gain.",0),
              end_twist("Result Twist",""), kineComp(k), q_err("Velocity Setpoints","")
        {}

        virtual bool componentStartup()
        {
            // Resolve the DataObjects :
            if ( !Base::Input::dObj()->Get("JointPositions", jpos_DObj) ||
                 !Base::Model::dObj()->Get("EndEffPosition", curFrame_DObj) ||
                 !Base::SetPoint::dObj()->Get("EndEffectorFrame", desFrame_DObj) ||
                 !Base::Output::dObj()->Get("JointVelocities", jvel_DObj) )
                return false;
            return true;
        }
            
        /**
         * @see KernelInterfaces.hpp class ModuleControlInterface
         */
        virtual void pull()      
        {
            jpos_DObj->Get( q6 );
            curFrame_DObj->Get( mp_base_frame );
            desFrame_DObj->Get( setpoint_frame );
                
            kineComp.stateSet( q6 );
        }
            
        /**
         * @see KernelInterfaces.hpp class ModuleControlInterface
         */
        virtual void calculate() 
        {
            // OK FOR ROTATIONAL PART OF TWIST :
            ee_base = mp_base_frame;
            //ee_base.p = Vector::Zero();
                
            kineComp.positionInverse(setpoint_frame, q_des );
            q_err = q_des - q6; 
            q_dot = q_err.value() * gain ;
            // ( goal_frame, current_frame )
#if 0
            mp_base_twist.vel = FrameDifference(setpoint_frame, mp_base_frame).vel*gain;
            mp_base_twist.rot = (ee_base * FrameDifference(setpoint_frame, mp_base_frame)).rot*gain;
#endif
        }
            
        /**
         * @see KernelInterfaces.hpp class ModuleControlInterface
         */
        virtual void push()      
        {
            jvel_DObj->Set( q_dot );
        }
            
        /**
         * @see KernelReportingExtension.hpp class ReportingComponent
         */
        virtual void exportReports(PropertyBag& bag)
        {
            bag.add(&q_err);
        }

        /**
         * @see KernelPropertyExtension.hpp class PropertyComponentInterface
         */
        virtual bool updateProperties( const PropertyBag& bag)
        {
            return composeProperty(bag, gain );
        }
            
        /**
         * @see KernelPropertyExtension.hpp class PropertyComponentInterface
         */
        virtual void exportProperties( PropertyBag& bag )
        {
            bag.add(&gain);
        }
            
    protected:
        Property<double> gain;
        Frame ee_base;
        Property<Twist> end_twist;
        Double6D q_des;
        KinematicsComponent kineComp;
        Property<Double6D> q_err;

        Double6D q6;
        typename Base::Input::DataObject<Double6D>::type* jpos_DObj;

        Frame mp_base_frame;
        typename Base::Model::DataObject<Frame>::type* curFrame_DObj;

        Frame setpoint_frame;
        typename Base::SetPoint::DataObject<Frame>::type* desFrame_DObj;

        Double6D q_dot;
        typename Base::Output::DataObject<Double6D>::type* jvel_DObj;
    };


    /**
     * A Cartesian Effector, can be used by the CartesianSensor for 'simulation' purposes.
     */
    template <class Base>
    class CartesianEffector
        : public Base
    {
    public:
        /**
         * Necessary typedefs.
         */
        typedef typename Base::OutputType OutputType;
            
        CartesianEffector(SimulatorInterface* _sim) 
            :  Base("CartesianEffector"),endTwist("Twist","The End Effector twist"), sim(_sim)
        {}

        virtual bool componentStartup()
        {
            if ( !Base::Output::dObj()->Get("JointVelocities",qdot_DObj) )
                return false;
            return true;
        }

        /**
         * @see KernelInterfaces.hpp class ModuleControlInterface
         */
        virtual void pull()      
        {
            qdot_DObj->Get( q_dot );
        }
            
        /**
         * @see KernelInterfaces.hpp class ModuleControlInterface
         */
        virtual void push()      
        {
            /*
             * Acces device drivers
             */
            if (sim !=0)
                sim->setQDots(q_dot, 0.05);
            //endTwist = output.mp_base_twist;
        }
            
        /**
         * @see KernelReportingExtension.hpp class ReportingComponent
         */
        virtual void exportReports(PropertyBag& bag)
        {
            bag.add(&endTwist);
        }

    protected:
        Property<Twist> endTwist;
        SimulatorInterface* sim;

        Double6D q_dot;
        typename Base::Output::DataObject<Double6D>::type* qdot_DObj;
    };

    /*
     */
    struct CartNSSensorInputs
        : public ServedTypes<Double6D>, public UnServedType< >
    {
        CartNSSensorInputs()
        {
            this->insert(make_pair(0,"JointPositions"));
        }
    };

    /**
     * A Fake Cartesian Sensor measuring all data sent by the CartesianEffector.
     */
    template <class Base>
    class CartesianSensor
        : public Base
    {
    public:
        /**
         * Necessary typedefs.
         */
        typedef typename Base::InputType InputType;
            
        CartesianSensor(SimulatorInterface* _sim = 0) 
            : Base("CartesianSensor"),
              sensorError(Event::SYNASYN, "CartesianSensor::SensorError"),
              q6("JointPositions",""), sim(_sim)
        {}
            
        virtual bool componentStartup()
        {
            if ( !Base::Input::dObj()->Get("JointPositions", jpos_DObj ) )
                return false;
            pull();
            push();
            return true;
        }
            
        /**
         * @see KernelInterfaces.hpp class ModuleControlInterface
         */
        virtual void pull()      
        {
            /*
             * Fake physical sensor data.
             */
            if (sim != 0)
                q6 = sim->getJointPositions();
            else
                q6 = 0;
        }

        /**
         * @see KernelInterfaces.hpp class ModuleControlInterface
         */
        virtual void push()      
        {
            jpos_DObj->Set( q6 );
        }

            
        /**
         * @see KernelReportingExtension.hpp class ReportingComponent
         */
        virtual void exportReports(PropertyBag& bag)
        {
            bag.add(&q6);
        }

            
    protected:
        Event sensorError;

        Property<Double6D> q6;
        typename Base::Input::DataObject<Double6D>::type* jpos_DObj;

        SimulatorInterface* sim;
    };

}
#endif

