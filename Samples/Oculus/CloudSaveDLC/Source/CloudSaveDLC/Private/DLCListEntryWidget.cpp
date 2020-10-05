#include "DLCListEntryWidget.h"
#include "OVRProduct.h"

void UDLCListEntryWidget::SetWidgetObject(UObject* object)
{
	Product = static_cast<UOVRProduct*>(object);
}

void UDLCListEntryWidget::PurchaseProduct()
{
	if (!Product || IsProductPurchased())
	{
		return;
	}
	Product->PurchaseProduct();
}

bool UDLCListEntryWidget::IsProductPurchased() const
{
	return Product->IsPurchased;
}

FString UDLCListEntryWidget::GetProductName() const
{
	return Product->Name;
}

FString UDLCListEntryWidget::GetProductPrice() const
{
	return Product->FormattedPrice;
}
