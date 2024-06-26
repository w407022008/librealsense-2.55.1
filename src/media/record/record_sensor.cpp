// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include "record_sensor.h"
#include "api.h"
#include "stream.h"
#include <src/depth-sensor.h>
#include <src/color-sensor.h>
#include <src/core/frame-callback.h>

#include <rsutils/string/from.h>

using namespace librealsense;

librealsense::record_sensor::record_sensor( device_interface& device,
                                            sensor_interface& sensor) :
    m_sensor(sensor),
    m_is_recording(false),
    m_parent_device(device),
    m_is_sensor_hooked(false),
    m_register_notification_to_base(true),
    m_before_start_callback_token(-1)
{
    LOG_DEBUG("Created record_sensor");
}

librealsense::record_sensor::~record_sensor()
{
    m_sensor.unregister_before_start_callback(m_before_start_callback_token);
    disable_sensor_options_recording();
    disable_sensor_hooks();
    m_is_recording = false;
    LOG_DEBUG("Destructed record_sensor");
}

void librealsense::record_sensor::init()
{
    //Seperating init from the constructor since callbacks may be called from here,
    // and the only way to register to them is after creating the record sensor

    enable_sensor_options_recording();
    m_before_start_callback_token = m_sensor.register_before_streaming_changes_callback([this](bool streaming)
    {
        if (streaming)
        {
            enable_sensor_hooks();
        }
        else
        {
            disable_sensor_hooks();
        }
    });

    if (m_sensor.is_streaming())
    {
        //In case the sensor is already streaming,
        // we will not get the above callback (before start) so we hook it now
        enable_sensor_hooks();
    }
    LOG_DEBUG("Hooked to real sense");
}
stream_profiles record_sensor::get_stream_profiles(int tag) const
{
    return m_sensor.get_stream_profiles(tag);
}

void librealsense::record_sensor::open(const stream_profiles& requests)
{
    m_sensor.open(requests);
}

void librealsense::record_sensor::close()
{
    m_sensor.close();
}

librealsense::option& librealsense::record_sensor::get_option(rs2_option id)
{
    return m_sensor.get_option(id);
}
const librealsense::option& librealsense::record_sensor::get_option(rs2_option id) const
{
    return m_sensor.get_option(id);
}
const std::string& librealsense::record_sensor::get_info(rs2_camera_info info) const
{
    return m_sensor.get_info(info);
}
bool librealsense::record_sensor::supports_info(rs2_camera_info info) const
{
    return m_sensor.supports_info(info);
}
bool librealsense::record_sensor::supports_option(rs2_option id) const
{
    return m_sensor.supports_option(id);
}

void librealsense::record_sensor::register_notifications_callback( rs2_notifications_callback_sptr callback )
{
    if (m_register_notification_to_base)
    {
        m_sensor.register_notifications_callback(std::move(callback)); //route to base sensor
        return;
    }

    m_user_notification_callback = std::move(callback);
    auto from_live_sensor = rs2_notifications_callback_sptr(new notification_callback([&](rs2_notification* n)
    {
        if (m_is_recording)
        {
            _on_notification( *( n->_notification ) );
        }
        if (m_user_notification_callback)
        {
            m_user_notification_callback->on_notification(n);
        }
    }), [](rs2_notifications_callback* p) { p->release(); });
    m_sensor.register_notifications_callback(std::move(from_live_sensor));
}

rs2_notifications_callback_sptr librealsense::record_sensor::get_notifications_callback() const
{
    return m_sensor.get_notifications_callback();
}

void librealsense::record_sensor::start( rs2_frame_callback_sptr callback )
{
    m_sensor.start(callback);
}
void librealsense::record_sensor::stop()
{
    m_sensor.stop();
}
bool librealsense::record_sensor::is_streaming() const
{
    return m_sensor.is_streaming();
}

bool librealsense::record_sensor::extend_to(rs2_extension extension_type, void** ext)
{
    /**************************************************************************************
     A record sensor wraps the live sensor, and should have the same functionalities.
     To do that, the record sensor implements the extendable_interface and when the user tries to
     treat the sensor as some extension, this function is called, and we return a pointer to the
     live sensor's extension. If that extension is a recordable one, we also enable_recording for it.
    **************************************************************************************/

    switch (extension_type)
    {
    case RS2_EXTENSION_OPTIONS: // [[fallthrough]]
    case RS2_EXTENSION_INFO:    // [[fallthrough]]
        *ext = this;
        return true;
    case RS2_EXTENSION_DEPTH_SENSOR:
        *ext = As< typename ExtensionToType< RS2_EXTENSION_DEPTH_SENSOR >::type >( &m_sensor );
        return *ext;
    case RS2_EXTENSION_DEPTH_STEREO_SENSOR:
        *ext = As< typename ExtensionToType< RS2_EXTENSION_DEPTH_STEREO_SENSOR >::type >( &m_sensor );
        return *ext;
    case RS2_EXTENSION_COLOR_SENSOR:
        *ext = As< typename ExtensionToType< RS2_EXTENSION_COLOR_SENSOR >::type >( &m_sensor );
        return *ext;
    case RS2_EXTENSION_MOTION_SENSOR:
        *ext = As< typename ExtensionToType< RS2_EXTENSION_MOTION_SENSOR >::type >( &m_sensor );
        return *ext;
    case RS2_EXTENSION_FISHEYE_SENSOR:
        *ext = As< typename ExtensionToType< RS2_EXTENSION_FISHEYE_SENSOR >::type >( &m_sensor );
        return *ext;
    case RS2_EXTENSION_POSE_SENSOR:
        *ext = As< typename ExtensionToType< RS2_EXTENSION_POSE_SENSOR >::type >( &m_sensor );
        return *ext;

    //Other extensions are not expected to be extensions of a sensor
    default:
        LOG_WARNING("Extensions type is unhandled: " << get_string(extension_type));
        return false;
    }
}

device_interface& record_sensor::get_device()
{
    return m_parent_device;
}

rs2_frame_callback_sptr record_sensor::get_frames_callback() const
{
    return m_frame_callback;
}

void record_sensor::set_frames_callback( rs2_frame_callback_sptr callback )
{
    m_frame_callback = callback;
}

stream_profiles record_sensor::get_active_streams() const
{
    return m_sensor.get_active_streams();
}

stream_profiles const & record_sensor::get_raw_stream_profiles() const
{
    return m_sensor.get_raw_stream_profiles();
}


int record_sensor::register_before_streaming_changes_callback(std::function<void(bool)> callback)
{
    throw librealsense::not_implemented_exception("playback_sensor::register_before_streaming_changes_callback");
}

void record_sensor::unregister_before_start_callback(int token)
{
    throw librealsense::not_implemented_exception("playback_sensor::unregister_before_start_callback");
}

template <typename T>
void librealsense::record_sensor::record_snapshot(rs2_extension extension_type, const librealsense::recordable<T>& ext)
{
    std::shared_ptr<T> snapshot;
    ext.create_snapshot(snapshot);
    auto ext_snapshot = As<extension_snapshot>(snapshot);
    if(m_is_recording)
    {
        //Send to recording thread
        _on_extension_change( extension_type, ext_snapshot );
    }
}

void record_sensor::stop_with_error(const std::string& error_msg)
{
    disable_recording();
    if (m_user_notification_callback)
    {
        std::string msg = rsutils::string::from() << "Stopping recording for sensor (streaming will continue). (Error: " << error_msg << ")";
        notification noti(RS2_NOTIFICATION_CATEGORY_UNKNOWN_ERROR, 0, RS2_LOG_SEVERITY_ERROR, msg);
        rs2_notification rs2_noti(&noti);
        m_user_notification_callback->on_notification(&rs2_noti);
    }
}

void record_sensor::disable_recording()
{
    m_is_recording = false;
}

processing_blocks librealsense::record_sensor::get_recommended_processing_blocks() const
{
    return m_sensor.get_recommended_processing_blocks();
}

void record_sensor::record_frame(frame_holder frame)
{
    if(m_is_recording)
    {
        //Send to recording thread
        _on_frame( std::move( frame ) );
    }
}

void record_sensor::enable_sensor_hooks()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_is_sensor_hooked)
        return;
    hook_sensor_callbacks();
    wrap_streams();
    m_is_sensor_hooked = true;
}
void record_sensor::disable_sensor_hooks()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_is_sensor_hooked)
        return;
    unhook_sensor_callbacks();
    m_is_sensor_hooked = false;
    m_register_notification_to_base = true;
}
void record_sensor::hook_sensor_callbacks()
{
    m_register_notification_to_base = false;
    m_user_notification_callback = m_sensor.get_notifications_callback();
    register_notifications_callback(m_user_notification_callback);
    m_original_callback = m_sensor.get_frames_callback();
    if (m_original_callback)
    {
        m_frame_callback = wrap_frame_callback(m_original_callback);
        m_sensor.set_frames_callback(m_frame_callback);
        m_is_recording = true;
    }
}
rs2_frame_callback_sptr librealsense::record_sensor::wrap_frame_callback( rs2_frame_callback_sptr callback )
{
    return make_frame_callback(
        [this, callback]( frame_holder frame )
        {
            record_frame( frame.clone() );

            // Raise to user callback
            frame_interface * ref = nullptr;
            std::swap( frame.frame, ref );
            callback->on_frame( (rs2_frame *)ref );
        } );
}
void record_sensor::unhook_sensor_callbacks()
{
    if (m_user_notification_callback)
    {
        m_sensor.register_notifications_callback(m_user_notification_callback);
    }

    if (m_original_callback)
    {
        m_sensor.set_frames_callback(m_original_callback);
        m_original_callback.reset();
    }
}
void record_sensor::enable_sensor_options_recording()
{
    for (int i = 0; i < static_cast<int>(RS2_OPTION_COUNT); i++)
    {
        rs2_option id = static_cast<rs2_option>(i);
        if (!m_sensor.supports_option(id))
        {
            continue;
        }

        if (m_recording_options.find(id) != m_recording_options.end())
        {
            continue;
        }

        try
        {
            auto& opt = m_sensor.get_option(id);
            opt.enable_recording([this, id](const librealsense::option& option) {
                options_container options;
                std::shared_ptr<librealsense::option> option_snapshot;
                option.create_snapshot(option_snapshot);
                options.register_option(id, option_snapshot);
                record_snapshot<options_interface>(RS2_EXTENSION_OPTIONS, options);
            });
            m_recording_options.insert(id);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Failed to enable recording for option " << get_string(id) << ", Error: " << e.what());
        }
    }
}
void record_sensor::disable_sensor_options_recording()
{
    for (auto id : m_recording_options)
    {
        auto& option = m_sensor.get_option(id);
        option.enable_recording([](const librealsense::option& snapshot) {});
    }
}
void record_sensor::wrap_streams()
{
    auto streams = m_sensor.get_active_streams();
    for (auto stream : streams)
    {
        auto id = stream->get_unique_id();
        if (m_recorded_streams_ids.count(id) == 0)
        {
            std::shared_ptr<stream_profile_interface> snapshot;
            stream->create_snapshot(snapshot);
            rs2_extension extension_type;
            if (Is<librealsense::video_stream_profile_interface>(stream))
                extension_type = RS2_EXTENSION_VIDEO_PROFILE;
            else if (Is<librealsense::motion_stream_profile_interface>(stream))
                extension_type = RS2_EXTENSION_MOTION_PROFILE;
            else if (Is<librealsense::pose_stream_profile_interface>(stream))
                extension_type = RS2_EXTENSION_POSE_PROFILE;
            else
                throw std::runtime_error("Unsupported stream");

            if( m_is_recording )
                _on_extension_change( extension_type, std::dynamic_pointer_cast< extension_snapshot >( snapshot ) );

            m_recorded_streams_ids.insert(id);
        }
    }
}
