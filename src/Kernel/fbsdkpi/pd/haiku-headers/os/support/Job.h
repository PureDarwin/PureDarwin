/*
 * Copyright 2011-2015, Haiku, Inc.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SUPPORT_JOB_H_
#define _SUPPORT_JOB_H_


#include <ObjectList.h>
#include <String.h>


namespace BSupportKit {


class BJob;


struct BJobStateListener {
	virtual						~BJobStateListener();

								// these default implementations do nothing
	virtual	void				JobStarted(BJob* job);
	virtual	void				JobProgress(BJob* job);
	virtual	void				JobSucceeded(BJob* job);
	virtual	void				JobFailed(BJob* job);
	virtual	void				JobAborted(BJob* job);
};


enum BJobState {
	B_JOB_STATE_WAITING_TO_RUN,
	B_JOB_STATE_STARTED,
	B_JOB_STATE_IN_PROGRESS,
	B_JOB_STATE_SUCCEEDED,
	B_JOB_STATE_FAILED,
	B_JOB_STATE_ABORTED,
};


class BJob {
public:
								BJob(const BString& title);
	virtual						~BJob();

			status_t			InitCheck() const;

	virtual	status_t			Run();

			const BString&		Title() const;
			BJobState			State() const;
			status_t			Result() const;
			const BString&		ErrorString() const;

			uint32				TicketNumber() const;

			status_t			AddStateListener(BJobStateListener* listener);
			status_t			RemoveStateListener(
									BJobStateListener* listener);

			bool				IsRunnable() const;
			status_t			AddDependency(BJob* job);
			status_t			RemoveDependency(BJob* job);
			int32				CountDependencies() const;

			BJob*				DependantJobAt(int32 index) const;

			class Private;

protected:
	virtual	status_t			Execute() = 0;
	virtual	void				Cleanup(status_t jobResult);

			void				SetState(BJobState state);
			void				SetErrorString(const BString&);

			void				NotifyStateListeners();

private:
	friend class Private;

			void				_SetTicketNumber(uint32 ticketNumber);
			void				_ClearTicketNumber();

private:
			status_t			fInitStatus;
			BString				fTitle;

			BJobState			fState;
			status_t			fResult;
			BString				fErrorString;

			uint32				fTicketNumber;

	typedef	BObjectList<BJob>	JobList;
			JobList				fDependencies;
			JobList				fDependantJobs;

	typedef	BObjectList<BJobStateListener>	StateListenerList;
			StateListenerList	fStateListeners;
};


}	// namespace BSupportKit


#endif // _SUPPORT_JOB_H_
