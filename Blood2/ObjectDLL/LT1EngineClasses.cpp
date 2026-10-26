// Engine object classes LT1 provided and Jupiter doesn't, so LT1 levels that place them still spawn them.
// The class is found by its C++ name, so it must be called Model, and it has its own file
// because the engine has an unrelated type with that name.

#include "cpp_engineobjects_de.h"
#include "cpp_server_de.h"


// LT1's Model from AppHeaders/engineobjects.c. The engine reads Filename, Skin and Flags itself
class Model : public DEBaseClass
{
public:

	Model() : DEBaseClass(OT_MODEL) {}

protected:

	virtual DDWORD EngineMessageFn(DDWORD messageID, void *pData, DFLOAT lData)
	{
		// LT1's mdl_MessageFn, which only sets the type on MID_PRECREATE
		if (messageID == MID_PRECREATE && pData)
		{
			((ObjectCreateStruct*)pData)->m_ObjectType = OT_MODEL;
		}

		return DEBaseClass::EngineMessageFn(messageID, pData, lData);
	}
};

BEGIN_CLASS(Model)
	ADD_STRINGPROP(Filename, "")
	ADD_STRINGPROP(Skin, "")
	ADD_LONGINTPROP(Flags, FLAG_VISIBLE)
END_CLASS_DEFAULT_FLAGS(Model, DEBaseClass, NULL, NULL, CF_ALWAYSLOAD)
