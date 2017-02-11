#ifndef INCLUDED_SLAMPROCESSENGINE_H
#define INCLUDED_SLAMPROCESSENGINE_H


namespace Slam
{

class SlamProcessEngine
{
  public:
    /// listener pertaining to SlamProcessEngine signal;
    class NewDataListener
    {
      public:

        /// When SlamProcessEngine starts off reading, this is called informing the listener
        /// @param[in]      success             the data reading successfully run.
        virtual void fisnihedProcessingData(bool success) = 0;
    };

    virtual ~SlamProcessEngine() {}

    /// @brief Handler to queue process request.
    /// @note This method is expected to be safe to call from any thread.
    ///
    /// @param[in]      listener  listener that needs be informed when the new data has been queued.
    virtual bool queueRequest(NewDataListener& listener) = 0;
    
  private:
    /// @brief Handler to initiate processing of a image data.
    /// @note   If the return is false, the image data has not started successfully.
    ///         If the data processing started successfully, a call will be made to the listener.
    /// @param[in]      listener      listener that needs be informed when the new data has been 
    ///                               queued.
    /// @return[out]    bool          true if the calculation starts successfully; false otherwise.
    virtual bool startProcessData(NewDataListener& listener) = 0;

};

} // close namepsace Slam

#endif
