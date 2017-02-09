#ifndef INCLUDED_EXECUTOR_H
#define INCLUDED_EXECUTOR_H


namespace Slam
{

class Executor
{
  public:
    /// listener pertaining to Executor actions;
    class CalculationListener
    {
      public:
        /// When Executor starts off calculation, this is called informing the listener
        /// @param[in]      success       the calculation successfully invoked to run.
        virtual void calculationFinished(bool success) = 0;
    };

    virtual ~Executor() {}

    /// @brief Handler to initiate calculation of a positionData.
    /// @note   If the return is false, the calculation has not started successfully.
    ///         If the calculation started successfully, a call will be made to the listener.
    /// @param[in]      listener            listener that needs be informed when the calculation has finished.
    /// @return[out]    bool                true if the calculation starts successfully; false otherwise.
    virtual bool startCalculation(CalculationListener& listener) = 0;

};

} // close namepsace Slam

#endif
