#include "wwkeyboard.h"

class WWKeyboardClassMorphOS : public WWKeyboardClass
{
public:
    virtual ~WWKeyboardClassMorphOS();

    virtual void Fill_Buffer_From_System(void);
    virtual KeyASCIIType To_ASCII(unsigned short num);

private:
};
