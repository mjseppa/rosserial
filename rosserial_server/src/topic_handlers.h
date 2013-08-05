
#include <ros/ros.h>
#include <rosserial_msgs/TopicInfo.h>
#include <rosserial_msgs/RequestMessageInfo.h>
#include <topic_tools/shape_shifter.h>


class Publisher {
public:
  Publisher(ros::NodeHandle& nh, const rosserial_msgs::TopicInfo& topic_info) {
    if (!message_service_.isValid()) {
      // lazy-initialize the service caller.
      message_service_ = nh.serviceClient<rosserial_msgs::RequestMessageInfo>("message_info");
      if (!message_service_.waitForExistence(ros::Duration(5.0))) {
        ROS_WARN("Timed out waiting for message_info service to become available.");
      }
    }

    rosserial_msgs::RequestMessageInfo info;
    info.request.type = topic_info.message_type;
    if (message_service_.call(info)) {
      if (info.response.md5 != topic_info.md5sum) {
        ROS_WARN_STREAM("Message" << topic_info.message_type  << "MD5 sum from client does not match that in system. Will avoid using system's message definition.");
        info.response.definition = "";
      }
    } else {
      ROS_WARN("Failed to call message_info service. Proceeding without full message definition.");
    }

    message_.morph(topic_info.md5sum, topic_info.message_type, info.response.definition, "false");
    publisher_ = message_.advertise(nh, topic_info.topic_name, 1);
  }

  void handle(ros::serialization::IStream stream) {
    ros::serialization::Serializer<topic_tools::ShapeShifter>::read(stream, message_);
    publisher_.publish(message_);
  }

private:
  ros::Publisher publisher_;
  topic_tools::ShapeShifter message_;

  static ros::ServiceClient message_service_;
};

ros::ServiceClient Publisher::message_service_;


class Subscriber {
public:
  Subscriber(ros::NodeHandle& nh, rosserial_msgs::TopicInfo& topic_info,
      boost::function<void(std::vector<uint8_t> buffer)> write_fn) 
    : write_fn_(write_fn) {
    ros::SubscribeOptions opts;
    opts.init<topic_tools::ShapeShifter>(
        topic_info.topic_name, 1, boost::bind(&Subscriber::handle, this, _1));
    opts.md5sum = topic_info.md5sum;
    opts.datatype = topic_info.message_type;
    subscriber_ = nh.subscribe(opts);
  }

private:
  void handle(const boost::shared_ptr<topic_tools::ShapeShifter const>& msg) {
    size_t length = ros::serialization::serializationLength(*msg);
    std::vector<uint8_t> buffer(length);

    ros::serialization::OStream ostream(&buffer[0], length);
    ros::serialization::Serializer<topic_tools::ShapeShifter>::write(ostream, *msg); 
 
    write_fn_(buffer);
  }

  ros::Subscriber subscriber_;
  boost::function<void(std::vector<uint8_t> buffer)> write_fn_;
};
