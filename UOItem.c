#include "UOItem.h"
#include "UOCallibrations.h"
#include "Hooks.h"
#include "BinaryTree.h"
#include "Allocation.h"
#include "UOItemList.h"

extern UOCallibrations * callibrations;
typedef void (__stdcall * pInitAOSString)(AOSString * stringobject, unsigned short * ustring);
pInitAOSString InitAOSString = NULL;
BinaryTree * items_by_id = NULL;

IItem _ItemVtbl = {
	DEFAULT_DISPATCH_VTBL,
	item_get_Id,
	item_put_Id,
	item_get_Type,
	item_put_Type,
	item_get_TypeIncrement,
	item_put_TypeIncrement,
	item_get_Position,
	item_put_Position,
	item_get_z,
	item_put_z,
	item_get_StackCount,
	item_put_StackCount,
	item_get_Color,
	item_put_Color,
	item_get_HighlightColor,
	item_put_HighlightColor,
	item_get_Direction,
	item_put_Direction,
	item_get_Flags,
	item_put_Flags,
	item_get_Name,
	item_put_Name,
	item_get_Properties,
	item_put_Properties,
	item_Click,
	item_DoubleClick,
	item_Drag,
	item_return_false,
	item_return_false,
	item_return_false
};

IContainer ContainerVtbl = {
	DEFAULT_DISPATCH_VTBL,
	item_get_Id,
	item_put_Id,
	item_get_Type,
	item_put_Type,
	item_get_TypeIncrement,
	item_put_TypeIncrement,
	item_get_Position,
	item_put_Position,
	item_get_z,
	item_put_z,
	item_get_StackCount,
	item_put_StackCount,
	item_get_Color,
	item_put_Color,
	item_get_HighlightColor,
	item_put_HighlightColor,
	item_get_Direction,
	item_put_Direction,
	item_get_Flags,
	item_put_Flags,
	item_get_Name,
	item_put_Name,
	item_get_Properties,
	item_put_Properties,
	item_Click,
	item_DoubleClick,
	item_Drag,
	item_return_false,
	item_return_true,
	item_return_false,
	container_IsOpen, 
	container_Open, 
	container_Content
	};

GUID * ItemGuids[3] = { (GUID *)&IID_IUnknown, (GUID *)&IID_IDispatch, (GUID *)&IID_IItem};

COMClass ItemClass = {
	"Item",								//name
	(GUID *)0,							//clsid
	2,									//default_iid;
	3,									//iids_count;
	ItemGuids,							//iids
	(GUID *)NULL,						//events_iid
	sizeof(Item),						//size
	&_ItemVtbl,							//lpVtbl
	(Structor)NULL,						//constructor
	(Structor)NULL,						//destructor
	0,									//typeinfo (init to 0)
	0,									//unused
	0,									//factory (init to 0)
	0,									//activeobject (init to 0)
	0									//instances (init to 0)
};

GUID * ContainerGuids[4] = { (GUID *)&IID_IUnknown, (GUID *)&IID_IDispatch, (GUID *)&IID_IItem, 
								(GUID *)&IID_IContainer};

COMClass ContainerClass = {
	"Container",						//name
	(GUID *)0,							//clsid
	2,									//default_iid;
	4,									//iids_count;
	ContainerGuids,						//iids
	(GUID *)NULL,						//events_iid
	sizeof(Item),						//size
	&ContainerVtbl,						//lpVtbl
	(Structor)NULL,						//constructor
	(Structor)NULL,						//destructor
	0,									//typeinfo (init to 0)
	0,									//unused
	0,									//factory (init to 0)
	0,									//activeobject (init to 0)
	0									//instances (init to 0)
};

#define GET_CONTAINER(itemoffset) (*((unsigned int *)((itemoffset) + callibrations->ItemOffsets.oContainer)))
 
Item * GetItemByID(unsigned int ID)
{
	Item * pNewItem = NULL;
	
	ClientItem * pClientItem;

	int isContainer = 0;
	int isMobile = 0;
	int isMulti = 0;

	unsigned int contents;

	if(pClientItem = item_get_pointers(ID))
	{
		/*
			a not so clean method to determine if an item is a container or not:
			We check if Item->contents points to a valid contained item, i.e. we check
			that item->contents[0]->container==item. The reason to do this is that 
			Item->Contents is non-zero for non-container items and can in fact be zero
			for container items if the container wasn't opened yet or the container is empty.
			This works, but there is no guarantee that some exotic item class might store
			data in Item->Contents which is not a pointer. In that case the Item->Contents->Container
			check could potentially cause a segfault. Haven't seen it so far, and item->contents
			always seems to contain the same value for non-container items. Still a better
			check would be appreciated.
		*/
		contents = *(pClientItem->Contents);
		if((contents!=0)&&(GET_CONTAINER(contents)!=pClientItem->self))
			isContainer=0;
		else//either empty or unopened container or one with valid contents
			isContainer=1;
	}

	if(isContainer)
		pNewItem = (Item *)ConstructObject(&ContainerClass);
	else
		pNewItem = (Item *)ConstructObject(&ItemClass);

	if(pNewItem)
		pNewItem->itemid = ID;

	return pNewItem;
}

Item * GetItemByOffset(unsigned int offset)
{
	unsigned int * pID;

	if(offset!=0)
	{
		pID = (unsigned int *)(offset + callibrations->ItemOffsets.oID);
		return GetItemByID(*pID);
	}

	return NULL;
}

int ID_compare(unsigned int a, unsigned int b)
{
	if(a>b)
		return 1;
	else if(a<b)
		return -1;
	else
		return 0;
}

int item_destructor_hook(RegisterStates * regs, int * bDestroy)
{
	unsigned int * pID;
	ClientItem * pFields;

	if( ((*bDestroy)!=0) && (regs->ecx!=0) )
	{
		pID = (unsigned int *)(regs->ecx + callibrations->ItemOffsets.oID);
		if(pFields = (ClientItem *)BT_remove(items_by_id, (void *)(*pID)))
			clean(pFields);
	}

	return 1;
}

ClientItem * item_get_pointers(unsigned int ID)
{
	ClientItem * toreturn = NULL;

	if(items_by_id == NULL)
		items_by_id = BT_create((BTCompare)ID_compare);

	if( (ID==0)||(ID==0xFFFFFFFF) )
		return NULL;

	if( (toreturn = (ClientItem *)BT_find(items_by_id, (void *)ID)) == NULL )
	{
		unsigned int itemoffset;

		if(itemoffset = (unsigned int)callibrations->FindItemByID(ID))
		{
			toreturn = create(ClientItem);

			toreturn->self = itemoffset;
			toreturn->Color = (unsigned short *)(itemoffset + callibrations->ItemOffsets.oColor);
			toreturn->Container = (unsigned int *)(itemoffset + callibrations->ItemOffsets.oContainer);
			toreturn->Contents = (unsigned int *)(itemoffset + callibrations->ItemOffsets.oContents);
			toreturn->ContentsNext = (unsigned int *)(itemoffset + callibrations->ItemOffsets.oContentsNext);
			toreturn->ContentsPrevious = (unsigned int *)(itemoffset + callibrations->ItemOffsets.oContentsPrevious);
			toreturn->Direction = (unsigned char *)(itemoffset + callibrations->ItemOffsets.oDirection);
			toreturn->Flags = (unsigned char *)(itemoffset + callibrations->ItemOffsets.oFlags);
			toreturn->Gump = (unsigned int *)(itemoffset + callibrations->ItemOffsets.oItemGump);
			toreturn->HighlightColor = (unsigned short *)(itemoffset + callibrations->ItemOffsets.oHighlightColor);
			toreturn->ID = (unsigned int *)(itemoffset + callibrations->ItemOffsets.oItemID);
			toreturn->Layers = (unsigned int *)(itemoffset + callibrations->ItemOffsets.oFirstLayer);
			toreturn->lpVtbl = *((ItemVtbl **)itemoffset);
			toreturn->MultiContents = (unsigned int *)(itemoffset + callibrations->ItemOffsets.oMultiContents);
			toreturn->MultiType = (unsigned short *)(itemoffset + callibrations->ItemOffsets.oMultiType);
			toreturn->NextFollower = (unsigned int *)(itemoffset + callibrations->ItemOffsets.oNextFollower);
			toreturn->Notoriety = (unsigned int *)(itemoffset + callibrations->ItemOffsets.oNotoriety);
			toreturn->Position = (Point2D *)(itemoffset + callibrations->ItemOffsets.oX);
			toreturn->StackCount = (unsigned char *)(itemoffset + callibrations->ItemOffsets.oStackCount);
			toreturn->Status = (unsigned int *)(itemoffset + callibrations->ItemOffsets.oMobileStatus);
			toreturn->Status2 = (unsigned int *)(itemoffset + callibrations->ItemOffsets.oMobileStatus2);
			toreturn->Type = (unsigned short *)(itemoffset + callibrations->ItemOffsets.oType);
			toreturn->TypeIncrement = (unsigned short *)(itemoffset + callibrations->ItemOffsets.oTypeIncrement);
			toreturn->z = (char *)(itemoffset + callibrations->ItemOffsets.oZ);

			/* insert into ID->fields tree */
			BT_insert(items_by_id, (void *)ID, (void *)toreturn);

			/* set destructor hook, so we can remove the fields structure when invalidated */
			SetVtblHook((void *)&(toreturn->lpVtbl->Destructor), (pHookFunc)item_destructor_hook, 4);
		}
	}
	
	return toreturn;
}

HRESULT __stdcall item_get_Id(Item * pThis, int * pID)
{
	(*pID) = pThis->itemid;
	return S_OK;
}

HRESULT __stdcall item_put_Id(Item * pThis, int ID)
{
	return S_OK;
}

HRESULT __stdcall item_get_Type(Item * pThis, int * pType)
{
	ClientItem * fields;

	(*pType) = 0;
	if(fields = item_get_pointers(pThis->itemid))
		(*pType) = *(fields->Type);

	return S_OK;
}

HRESULT __stdcall item_put_Type(Item * pThis, int Type)
{
	return S_OK;
}

HRESULT __stdcall item_get_TypeIncrement(Item * pThis, int * pTypeIncrement)
{
	ClientItem * fields;

	(*pTypeIncrement) = 0;
	if(fields = item_get_pointers(pThis->itemid))
		(*pTypeIncrement) = *(fields->TypeIncrement);

	return S_OK;
}

HRESULT __stdcall item_put_TypeIncrement(Item * pThis, int TypeIncrement)
{
	return S_OK;
}

HRESULT __stdcall item_get_Position(Item * pThis, Point2D * pPos)
{
	ClientItem * fields;

	(*pPos).x=0; (*pPos).y=0;
	if(fields = item_get_pointers(pThis->itemid))
		(*pPos) = *(fields->Position);

	return S_OK;
}

HRESULT __stdcall item_put_Position(Item * pThis, Point2D pos)
{
	return S_OK;
}

HRESULT __stdcall item_get_z(Item * pThis, int * pPos)
{
	ClientItem * fields;

	(*pPos) = 0;
	if(fields = item_get_pointers(pThis->itemid))
		(*pPos) = *(fields->z);
	return S_OK;
}

HRESULT __stdcall item_put_z(Item * pThis, int pos)
{
	return S_OK;
}

HRESULT __stdcall item_get_StackCount(Item * pThis, int * pCount)
{
	ClientItem * fields;

	(*pCount) = 0;
	if(fields = item_get_pointers(pThis->itemid))
		(*pCount) = *(fields->StackCount);

	return S_OK;
}

HRESULT __stdcall item_put_StackCount(Item * pThis, int count)
{
	return S_OK;
}

HRESULT __stdcall item_get_Color(Item * pThis, int * pColor)
{
	ClientItem * fields;

	(*pColor) = 0;
	if(fields = item_get_pointers(pThis->itemid))
		(*pColor) = *(fields->Color);

	return S_OK;
}

HRESULT __stdcall item_put_Color(Item * pThis, int color)
{
	return S_OK;
}

HRESULT __stdcall item_get_HighlightColor(Item * pThis, int * pHighlightColor)
{
	ClientItem * fields;

	(*pHighlightColor) = 0;
	if(fields = item_get_pointers(pThis->itemid))
		(*pHighlightColor) = *(fields->HighlightColor);

	return S_OK;
}

HRESULT __stdcall item_put_HighlightColor(Item * pThis, int highlightcolor)
{
	return S_OK;
}

HRESULT __stdcall item_get_Direction(Item * pThis, int * pDirection)
{
	ClientItem * fields;

	(*pDirection) = 0;
	if(fields = item_get_pointers(pThis->itemid))
		(*pDirection) = *(fields->Direction);

	return S_OK;
}

HRESULT __stdcall item_put_Direction(Item * pThis, int direction)
{
	return S_OK;
}

HRESULT __stdcall item_get_Flags(Item * pThis, int * pFlags)
{
	ClientItem * fields;

	(*pFlags) = 0;
	if(fields = item_get_pointers(pThis->itemid))
		(*pFlags) = *(fields->Flags);

	return S_OK;
}

HRESULT __stdcall item_put_Flags(Item * pThis, int Flags)
{
	return S_OK;
}

HRESULT __stdcall item_get_Name(Item * pThis, BSTR * pName)
{
	AOSString _str;
	char * temp;

	if(InitAOSString==NULL)
		InitAOSString = (pInitAOSString)create_thiscall_wrapper((unsigned int)(callibrations->InitString));

	memset(&_str, 0, sizeof(AOSString));
	InitAOSString(&_str, callibrations->DefaultNameString);
	callibrations->GetName(pThis->itemid, &_str, 0, 1);
	if(_str.pString)
	{
		temp = ole2char((LPOLESTR)_str.pString);
		printf("name -> %s\n", temp);
		clean(temp);
		(*pName) = SysAllocString((const OLECHAR *)_str.pString);
	}
	else
	{
		(*pName) = SysAllocString(L"");
	}

	return S_OK;
}

HRESULT __stdcall item_put_Name(Item * pThis, BSTR strName)
{
	SysFreeString(strName);
	return S_OK;
}

HRESULT __stdcall item_get_Properties(Item * pThis, BSTR * pProperties)
{
	AOSString _str;

	if(InitAOSString==NULL)
		InitAOSString = (pInitAOSString)create_thiscall_wrapper((unsigned int)(callibrations->InitString));

	memset(&_str, 0, sizeof(AOSString));
	InitAOSString(&_str, callibrations->DefaultPropertiesString);
	callibrations->GetProperties(pThis->itemid, &_str, 0, 1);
	if(_str.pString)
		(*pProperties) = SysAllocString((const OLECHAR *)_str.pString);
	else
		(*pProperties) = SysAllocString(L"");

	return S_OK;
}

HRESULT __stdcall item_put_Properties(Item * pThis, BSTR strProperties)
{
	SysFreeString(strProperties);
	return S_OK;
}

HRESULT __stdcall item_Click(Item * pThis)
{
	return S_OK;
}

HRESULT __stdcall item_DoubleClick(Item * pThis)
{
	return S_OK;
}

HRESULT __stdcall item_Drag(Item * pThis)
{
	return S_OK;
}

HRESULT __stdcall item_return_false(Item * pThis, VARIANT_BOOL * bResult)
{
	(*bResult) = VARIANT_FALSE;
	return S_OK;
}

HRESULT __stdcall item_return_true(Item * pThis, VARIANT_BOOL * bResult)
{
	(*bResult) = VARIANT_TRUE;
	return S_OK;
}

HRESULT __stdcall container_IsOpen(Item * pThis, VARIANT_BOOL * bIsOpen)
{
	ClientItem * pci;
	(*bIsOpen)=VARIANT_FALSE;
	if(pci=item_get_pointers(pThis->itemid))
	{
		if((*(pci->Gump))!=0)
			(*bIsOpen) = VARIANT_TRUE;
	}
	return S_OK;
}

HRESULT __stdcall container_Open(Item * pThis, VARIANT_BOOL * bOpened)
{
	unsigned int i;
	container_IsOpen(pThis, bOpened);
	
	if((*bOpened)==VARIANT_TRUE)
		return S_OK;

	item_DoubleClick(pThis);

	/* TODO: wait for gump open? */
	i=0;
	while(((*bOpened)==VARIANT_FALSE)&&(i<10))
	{
		callibrations->GeneralPurposeFunction(1);
		container_IsOpen(pThis, bOpened);
		i++;
	}

	return S_OK;
}

HRESULT __stdcall container_Content(Item * pThis, void ** pContents)
{
	(*pContents) = (void *)create_itemlist(pThis->itemid);
	return S_OK;
}