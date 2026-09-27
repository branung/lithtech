// ----------------------------------------------------------------------- //
//
// MODULE  : DialogTrigger.h
//
// PURPOSE : DialogTrigger - Definition
//
// CREATED : 1/26/98
//
// ----------------------------------------------------------------------- //

#ifndef __DIALOG_TRIGGER_H__
#define __DIALOG_TRIGGER_H__

#include "cpp_engineobjects_de.h"
#include "Activation.h"

#define MAX_MESSAGES_NUM	3

class DialogTrigger : public DEBaseClass
{
	public :

		DialogTrigger();
		~DialogTrigger();

				void	Trigger (int nSelection);

		/*
			Dialog registry

			The dialog round trip names its trigger by a 32-bit ID (MID_COMMAND_SHOWDLG out, MID_DIALOG_CLOSE back).
			An ID can't name a freed object, and one the server never issued is rejected.
		*/
		static DialogTrigger*	FromDialogID (uint32 nID);
				uint32			GetDialogID () const { return m_nDialogID; }

	protected :

		virtual DDWORD	EngineMessageFn(DDWORD messageID, void *pData, DFLOAT fData);
		virtual DDWORD	ObjectMessageFn(HOBJECT hSender, DDWORD messageID, HMESSAGEREAD hRead);

		virtual DBOOL	ReadProp(ObjectCreateStruct *pInfo);
		virtual void	ObjectTouch(HOBJECT hObj);

				void	ShowDialog();

	protected:

		CActivation		m_activation;
		
		DBOOL			m_bTriggered;
		DBOOL			m_bFirstUpdate;

		HOBJECT			m_hPlayerObject;
		DDWORD			m_nStringID[MAX_MESSAGES_NUM];
		HSTRING			m_hTarget[MAX_MESSAGES_NUM];
		HSTRING			m_hMessage[MAX_MESSAGES_NUM];
		DFLOAT			m_fSendDelay;
		DVector			m_vDims;

		uint32			m_nDialogID;
		

	private :

		void Save(HMESSAGEWRITE hWrite, DDWORD dwSaveFlags);
		void Load(HMESSAGEREAD hRead, DDWORD dwLoadFlags);

		void Trigger();
};

#endif // __DIALOG_TRIGGER_H__