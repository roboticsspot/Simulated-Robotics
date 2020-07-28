#include <gazebo/gazebo.hh>
#include <gazebo/physics/physics.hh>
#include <ignition/math/Pose3.hh>
#include <ignition/math/Vector3.hh>
#include <string_view>
#include <gazebo/physics/Collision.hh>
#include <gazebo/sensors/sensors.hh>

namespace gazebo
{
  class WorldUuvPlugin : public WorldPlugin
  {

  enum states{
    unconnectable_unlocked, connectable_unlocked, connectable_locked
  };

  // private: states state = unconnectable_unlcoked;

  private: physics::WorldPtr world;

  private: physics::ModelPtr plugModel;

  private: physics::LinkPtr plugLink;

  private: physics::ModelPtr socketModel;

  private: physics::LinkPtr socketLink;

  private: ignition::math::Pose3d socket_pose;

  private: ignition::math::Pose3d plug_pose;

  private: physics::JointPtr prismaticJoint;

  public: ignition::math::Vector3d grabAxis;

  public: ignition::math::Vector3d grabbedForce;

  private: bool joined = false;
  
  private: bool unlocked = true;

  private: gazebo::event::ConnectionPtr updateConnection;


  public: WorldUuvPlugin() : WorldPlugin(){}

  public: void Load(physics::WorldPtr _world, sdf::ElementPtr _sdf){
      this->world = _world;
      this->socketModel = this->world->ModelByName("socket_bar");
      this->plugModel = this->world->ModelByName("grab_bar");
      this->socketLink = this->socketModel->GetLink("tube");
      this->plugLink = this->plugModel->GetLink("bar");
      this->updateConnection = gazebo::event::Events::ConnectWorldUpdateBegin(
          std::bind(&WorldUuvPlugin::Update, this));
    }

  private: bool checkRollAlignment(){
      ignition::math::Vector3<double> socketRotation = socketModel->RelativePose().Rot().Euler();
      ignition::math::Vector3<double> plugRotation = plugModel->RelativePose().Rot().Euler();
      return abs(plugRotation[0] - socketRotation[0]) < 0.1;
    }

  private: bool checkPitchAlignment(){
      ignition::math::Vector3<double> socketRotation = socketModel->RelativePose().Rot().Euler();
      ignition::math::Vector3<double> plugRotation = plugModel->RelativePose().Rot().Euler();
      return abs(plugRotation[1] - socketRotation[1]) < 0.1;
    }

  private:bool checkYawAlignment(){
      ignition::math::Vector3<double> socketRotation = socketModel->RelativePose().Rot().Euler();
      ignition::math::Vector3<double> plugRotation = plugModel->RelativePose().Rot().Euler();
      return abs(plugRotation[2] - socketRotation[2]) < 0.1;
    }

  private: bool checkRotationalAlignment()
    {
      if (this->checkYawAlignment() && this->checkPitchAlignment() && this->checkRollAlignment())
      {
        // printf("Aligned, ready for insertion  \n");
        return true;
      }
      else
      {
        // printf("Disaligned, not ready for mating  \n");
        return false;
      }
    }

  private: bool checkVerticalAlignment()
    {
      socket_pose = socketModel->RelativePose();
      ignition::math::Vector3<double> socketPositon = socket_pose.Pos();
      // printf("%s  \n", typeid(socketPositon).name());

      plug_pose = plugModel->RelativePose();
      ignition::math::Vector3<double> plugPosition = plug_pose.Pos();

      bool onSameVerticalLevel = abs(plugPosition[2] - socketPositon[2]) < 0.1;
      if (onSameVerticalLevel)
      {
        // printf("z=%.2f  \n", plugPosition[2]);
        // printf("Share same vertical level  \n");
        return true;
      }
      return false;
    }

  private: bool isAlligned()
    {
      if(checkVerticalAlignment() && checkRotationalAlignment()){
        return true;
      } else {
        return false;
      }
    }

  private: bool checkProximity()
    {
      socket_pose = socketModel->WorldPose();
      // socket_pose = socketLink->RelativePose();
      ignition::math::Vector3<double> socketPositon = socket_pose.Pos();
      // printf("%s  \n", typeid(socketPositon).name());

      plug_pose = plugModel->RelativePose();
      ignition::math::Vector3<double> plugPosition = plug_pose.Pos();
      // printf("%f %f %f Within Proximity  \n", plugPosition[0], plugPosition[1], plugPosition[2]);
      float xdiff_squared = pow(abs(plugPosition[0] - socketPositon[0]),2);
      float ydiff_squared = pow(abs(plugPosition[1] - socketPositon[1]),2);
      float zdiff_squared = pow(abs(plugPosition[2] - socketPositon[2]),2);

      bool withinProximity = pow(xdiff_squared+ydiff_squared+zdiff_squared,0.5) < 1;
      if (withinProximity)
      {
        // printf("%f Within Proximity  \n", plugPosition[0]);
        return true;
      }else {

        // printf("not within Proximity  \n");
      }
      return false;
    }

  private: bool isReadyForInsertion()
    {
      if (this->checkProximity() && this->isAlligned()){
        return true;
        // printf("Insert \n");

      } else{

        // printf("Cant insert \n");
        return false;
      }
    }

  public: void freezeJoint(physics::JointPtr prismaticJoint){
    double currentPosition = prismaticJoint->Position(0);
    prismaticJoint->SetUpperLimit(0, currentPosition);
    prismaticJoint->SetLowerLimit(0, currentPosition);
  }
  
  public: void Update()
    {
      // connect the socket and the plug after 5 seconds
      if (this->world->SimTime() > 10.0 && joined == false)
      {
        this->joined = true;
        this->prismaticJoint = plugModel->CreateJoint(
          "socket_plug_prismatic_joint",
          "prismatic",
          socketLink,
          plugLink);
        prismaticJoint->Load(this->socketLink, this->plugLink, 
          ignition::math::Pose3<double>(ignition::math::Vector3<double>(1, 0, 0), 
          ignition::math::Quaternion<double>(0, 0, 0, 0)));
        prismaticJoint->Init();
        prismaticJoint->SetAxis(0, ignition::math::Vector3<double>(1, 0, 0));
        socketLink->SetCollideMode("all");
        plugLink->SetCollideMode("all");
      }
      if (joined && unlocked){
        plugModel->SetSelfCollide(true);
        grabAxis = prismaticJoint->LocalAxis(0);
        if (true){
          grabbedForce = socketLink->RelativeForce();
          if (abs(grabbedForce[0])>=0){
            double joint_force = prismaticJoint->GetForce(0);
            printf("%.2f %.2f %.2f ", abs(grabbedForce[0]), abs(grabbedForce[1]), abs(grabbedForce[2]));
            printf("%.2f \n", joint_force);
          }
        }
      }
    }
  };
  GZ_REGISTER_WORLD_PLUGIN(WorldUuvPlugin)
} // namespace gazebo
