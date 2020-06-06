// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineStorePS4.h"
#include "OnlineSubsystemPS4.h"
#include "OnlineAsyncTaskManagerPS4.h"
#include "OnlineIdentityInterfacePS4.h"

class FStoreTaskBase : public FOnlineAsyncTask
{
public:
	FStoreTaskBase()
		: OnlineStore(nullptr)
		, bIsComplete(false)
		, bWasSuccessful(false)
	{
	}

	virtual void BeginOperation() = 0;
	virtual bool IsDone() const override { return bIsComplete; }
	virtual bool WasSuccessful() const override { return bWasSuccessful; }

	void FailTask(const FString& Error)
	{
		UE_LOG_ONLINE_STOREV2(Error, TEXT("FOnlineStorePS4: %s"), *Error);

		bIsComplete = true;
		bWasSuccessful = false;
		ErrorString = Error;
	}

	FOnlineStorePS4* OnlineStore;
	FString ErrorString;
	bool bIsComplete;
	bool bWasSuccessful;
};

template<typename ResultType>
class TStoreTaskWithResponse : public FStoreTaskBase
{
public:
	TStoreTaskWithResponse(const TCHAR* FuncNameStatic)
	: FuncName(FuncNameStatic)
	{
	}
	typedef TStoreTaskWithResponse<ResultType> Parent;

	void ReportError(int32 ErrorCode, const TCHAR* Context)
	{
		FailTask(FString::Printf(TEXT("%s returned 0x%08x (%s)"), FuncName, ErrorCode, Context));
	}

	virtual FString ToString() const { return FString::Printf(TEXT("Call to %s"), FuncName); }

	enum class EStoreTaskCompletionStatus : uint8
	{
		/** Completed successfully */
		CompletedSuccessfully,
		/** Completed with failure */
		CompletedWithFailure,
		/** Not complete yet, more requests are queued */
		Pending
	};

	virtual void Tick() override
	{
		// see if it's done
		if (Response.isLocked())
		{
			// keep waiting
			return;
		}

		// check for error
		if (Response.getReturnCode() != SCE_TOOLKIT_NP_V2_SUCCESS)
		{
			ReportError(Response.getReturnCode(), TEXT("async"));
			return;
		}

		// get the result
		const ResultType* Result = Response.get();
		if (Result == nullptr)
		{
			FailTask(FString::Printf(TEXT("%s future returned null"), FuncName));
			return;
		}

		// forward to handler
		EStoreTaskCompletionStatus CompletionStatus = HandleResult(*Result);
		switch (CompletionStatus)
		{
		case EStoreTaskCompletionStatus::CompletedSuccessfully:
			bWasSuccessful = true;
			bIsComplete = true;
			break;
		case EStoreTaskCompletionStatus::CompletedWithFailure:
			bWasSuccessful = false;
			bIsComplete = true;
			break;
		case EStoreTaskCompletionStatus::Pending:
			// The subtask must have another request in flight now
			break;
		default:
			checkNoEntry();
		}
	}

	virtual EStoreTaskCompletionStatus HandleResult(const ResultType& Result) = 0;

	NpToolkit::Core::Response<ResultType> Response;
	const TCHAR* const FuncName;
};

FOnlineStorePS4::FOnlineStorePS4(FOnlineSubsystemPS4* InPS4Subsystem)
	: Ps4Subsystem(InPS4Subsystem)
{
	check(Ps4Subsystem != nullptr);
}

FOnlineStorePS4::~FOnlineStorePS4()
{
}

void FOnlineStorePS4::QueueStoreTask(FStoreTaskBase* Task)
{
	Task->OnlineStore = this;
	Task->BeginOperation();
	if (Task->IsDone())
	{
		// schedule finalize for next tick then delete
		Ps4Subsystem->ExecuteNextTick([Task]() {
			Task->Finalize();
			delete Task;
		});
		return;
	}

	// queue for async 
	FOnlineAsyncTaskManagerPS4 * AsyncTaskManager = Ps4Subsystem->GetAsyncTaskManager();
	check(AsyncTaskManager);
	AsyncTaskManager->AddToInQueue(Task);
}

class FQueryCategoriesOp : public TStoreTaskWithResponse<NpToolkit::Commerce::Categories>
{
public:
	FQueryCategoriesOp(FUniqueNetIdPS4 const& LocalUserId, FOnQueryOnlineStoreCategoriesComplete const& InDelegate)
		: Parent(TEXT("Commerce::Interface::getCategoryInfo"))
		, Delegate(InDelegate)
	{
		Request.userId = LocalUserId.GetUserId();
		Request.serviceLabel = SCE_NP_DEFAULT_SERVICE_LABEL;
		Request.async = true;

		Request.numCategories = 0; // root category
	}

	virtual void BeginOperation() override
	{
		// spawn the task
		int32 Ret = NpToolkit::Commerce::getCategories(Request, &Response);
		if (Ret < SCE_TOOLKIT_NP_V2_SUCCESS)
		{
			return ReportError(Ret, TEXT("sync"));
		}
	}

	virtual EStoreTaskCompletionStatus HandleResult(const NpToolkit::Commerce::Categories& Result) override
	{
		for (int32 CategoryIndex = 0; CategoryIndex < Result.numCategories; ++CategoryIndex)
		{
			auto const& CurrentCategory = Result.categories[CategoryIndex];

			FOnlineStoreCategory Category;
			Category.Id = ANSI_TO_TCHAR(CurrentCategory.categoryLabel.value);
			Category.Description = FText::FromString(ANSI_TO_TCHAR(CurrentCategory.categoryDescription));

			for (int32 SubCategoryIndex = 0; SubCategoryIndex < CurrentCategory.numSubCategories; ++SubCategoryIndex)
			{
				FOnlineStoreCategory& SubCategory = *new (Category.SubCategories) FOnlineStoreCategory;
				auto const& CurrentSubCategory = CurrentCategory.subCategories[SubCategoryIndex];

				SubCategory.Id = ANSI_TO_TCHAR(CurrentSubCategory.categoryLabel.value);
				SubCategory.Description = Category.Description;
			}

			CategoryList.Add(Category);
		}

		return EStoreTaskCompletionStatus::CompletedSuccessfully;
	}

	virtual void Finalize() override
	{
		if (bWasSuccessful)
		{
			OnlineStore->CachedCategories = CategoryList;
		}
		Delegate.ExecuteIfBound(bWasSuccessful, ErrorString);
	}

	NpToolkit::Commerce::Request::GetCategories Request;
	FOnQueryOnlineStoreCategoriesComplete Delegate;
	TArray<FOnlineStoreCategory> CategoryList;
};

void FOnlineStorePS4::QueryCategories(const FUniqueNetId& UserId, const FOnQueryOnlineStoreCategoriesComplete& Delegate)
{
	QueueStoreTask(new FQueryCategoriesOp(FUniqueNetIdPS4::Cast(UserId), Delegate));
}

class FGetProductInfoListOp
	: public TStoreTaskWithResponse<NpToolkit::Commerce::Products>
{
	const int32 MaxOffersPerRequest = NpToolkit::Commerce::Request::GetProducts::MAX_PRODUCTS;
public:
	FGetProductInfoListOp(const FUniqueNetIdPS4& LocalUserId, const TArray<FUniqueOfferId>& InOfferIds, const FOnQueryOnlineStoreOffersComplete& InDelegate)
		: Parent(TEXT("Commerce::Interface::ProductInfoDetailedList"))
		, Delegate(InDelegate)
		, RequestedOfferIds(InOfferIds)
	{
		const int32 NumRequestsNeeded = (RequestedOfferIds.Num() + MaxOffersPerRequest - 1) / MaxOffersPerRequest;
		Requests.SetNum(NumRequestsNeeded);

		UE_LOG_ONLINE_STOREV2(Log, TEXT("Querying PS4 Store for %d Offers with %d Requests"), RequestedOfferIds.Num(), NumRequestsNeeded);

		for (int32 RequestIndex = 0; RequestIndex < NumRequestsNeeded; ++RequestIndex)
		{
			const int32 OfferIndexStart = RequestIndex * MaxOffersPerRequest;
			const int32 OfferIndexEnd = FMath::Min((RequestIndex + 1) * MaxOffersPerRequest, RequestedOfferIds.Num());
			const int32 NumOffersThisRequest = OfferIndexEnd - OfferIndexStart;

			NpToolkit::Commerce::Request::GetProducts& Request = Requests[RequestIndex];
			Request.userId = LocalUserId.GetUserId();
			Request.serviceLabel = SCE_NP_DEFAULT_SERVICE_LABEL;
			Request.async = true;

			Request.keepHtmlTags = true; // apparently if we tell PS4 to strip HTML we lose our linebreaks as well. SMH
			Request.numProducts = NumOffersThisRequest;

			// Fill in the product IDs array
			for (int32 ProductLabelIndex = 0; ProductLabelIndex < NumOffersThisRequest; ++ProductLabelIndex)
			{
				const FString& OfferId = RequestedOfferIds[OfferIndexStart + ProductLabelIndex];
				UE_LOG_ONLINE_STOREV2(Log, TEXT("  Offer %d Request %d: [%s]"), OfferIndexStart + ProductLabelIndex, RequestIndex, *OfferId);

				constexpr const int32 NullByteLength = 1;
				static_assert(UE_ARRAY_COUNT(NpToolkit::Commerce::ProductLabel::value) == (NpToolkit::Commerce::ProductLabel::PRODUCT_LABEL_MAX_LEN + NullByteLength), "Product Label value must fit max length + null byte");

				NpToolkit::Commerce::ProductLabel& ProductIdParam = Request.productLabels[ProductLabelIndex];
				FCStringAnsi::Strncpy(ProductIdParam.value, TCHAR_TO_ANSI(*OfferId), NpToolkit::Commerce::ProductLabel::PRODUCT_LABEL_MAX_LEN + NullByteLength);
			}
		}
	}

	virtual void BeginOperation() override
	{
		if (LIKELY(Requests.IsValidIndex(CurrentRequestIndex)))
		{
			StartNextRequest();
		}
		else
		{
			bWasSuccessful = true;
			bIsComplete = true;
		}
	}
	
	void StartNextRequest()
	{
		ensure(Requests.IsValidIndex(CurrentRequestIndex));
		NpToolkit::Commerce::Request::GetProducts& Request = Requests[CurrentRequestIndex];
		int32 Ret = NpToolkit::Commerce::getProducts(Request, &Response);
		if (Ret < SCE_TOOLKIT_NP_V2_SUCCESS)
		{
			ReportError(Ret, TEXT("sync"));
		}
	}

	virtual EStoreTaskCompletionStatus HandleResult(NpToolkit::Commerce::Products const& Result) override
	{
		UE_LOG_ONLINE_STOREV2(Log, TEXT("PS4 Store Found %d Offers"), Result.numProducts);
		check(Requests.IsValidIndex(CurrentRequestIndex));
		const int32 ExpectedNumProducts = Requests[CurrentRequestIndex].numProducts;
		const bool bWrongOfferCount = Result.numProducts != ExpectedNumProducts;

		// Unique list of offer ids we have been returned.  This is only populated if we are returned
		// a different amount of offers than we requested
		TSet<FString> FoundOfferIds;

		for (int32 ProductIndex = 0; ProductIndex < Result.numProducts; ++ProductIndex)
		{
			const NpToolkit::Commerce::Product& Product = Result.products[ProductIndex];

			FOnlineStoreOfferRef Offer = MakeShared<FOnlineStoreOffer>();
			Offer->OfferId = FString(ANSI_TO_TCHAR(Product.productLabel.value));
			Offer->Title = ConvertSonyStoreText(Product.productName);
			Offer->Description = Offer->Title;

			// Keep track of offers we found, if we didn't get the correct amount of offers back
			if (bWrongOfferCount)
			{
				FoundOfferIds.Add(Offer->OfferId);
			}

			UE_LOG_ONLINE_STOREV2(Log, TEXT("  Offer ID: %s"), *Offer->OfferId);
			UE_LOG_ONLINE_STOREV2(Log, TEXT("  Title: %s"), *Offer->Title.ToString());
			UE_LOG_ONLINE_STOREV2(Verbose, TEXT("  Short Description: %s"), *Offer->Description.ToString().ReplaceCharWithEscapedChar());
			UE_LOG_ONLINE_STOREV2(VeryVerbose, TEXT("  Original Short Description: %s"), ANSI_TO_TCHAR(Product.productName));

			// TODO: Some products are returning valid details, but with this field set to false.
			//if (Product.hasDetails)
			{
				const NpToolkit::Commerce::ProductDetails& Details = Product.details;
				Offer->LongDescription = ConvertSonyStoreText(Details.longDescription);

				UE_LOG_ONLINE_STOREV2(VeryVerbose, TEXT("  Long Description: %s"), *Offer->LongDescription.ToString().ReplaceCharWithEscapedChar());
				UE_LOG_ONLINE_STOREV2(VeryVerbose, TEXT("  Original Long Description: %s"), ANSI_TO_TCHAR(Product.details.longDescription));

				if (Details.numSkus >= 1)
				{
					// TODO: Support more than one SKU.
					const NpToolkit::Commerce::SkuInfo& SKU = Details.skuinfo[0];

					Offer->NumericPrice = SKU.intPrice;
					Offer->PriceText = ConvertSonyStoreText(SKU.price);
					int32 Offset = FCStringAnsi::Strlen(SKU.price) - 3; // where is the 3 character currency code
					if (Offset >= 0 && FCharAnsi::IsAlpha(SKU.price[Offset]) && FCharAnsi::IsAlpha(SKU.price[Offset + 1]) && FCharAnsi::IsAlpha(SKU.price[Offset + 2]))
					{
						Offer->CurrencyCode = FString(ANSI_TO_TCHAR(SKU.price + Offset));
					}
				}

				SceRtcDateTime ReleaseDateTime = {};
				sceRtcSetTick(&ReleaseDateTime, &Details.releaseDate);

				Offer->ReleaseDate = FDateTime(
					ReleaseDateTime.year,
					ReleaseDateTime.month,
					ReleaseDateTime.day,
					ReleaseDateTime.hour,
					ReleaseDateTime.minute,
					ReleaseDateTime.second,
					ReleaseDateTime.microsecond / 1000); // Convert micro to milli seconds.
				UE_LOG_ONLINE_STOREV2(VeryVerbose, TEXT("  Release Date: %s"), *Offer->ReleaseDate.ToString());
			}

			UE_LOG_ONLINE_STOREV2(Verbose, TEXT("  Numeric Price: %d"), Offer->NumericPrice);
			UE_LOG_ONLINE_STOREV2(Verbose, TEXT("  Price Text: %s"), *Offer->PriceText.ToString());
			UE_LOG_ONLINE_STOREV2(Verbose, TEXT("  Country Code: %s"), *Offer->CurrencyCode);

			OffersList.Add(Offer);
		}

		if (bWrongOfferCount)
		{
			UE_LOG_ONLINE_STOREV2(Warning, TEXT("Incorrect amount of PS4 Store offers found; expected %d, found %d offers"), ExpectedNumProducts, Result.numProducts);
			const int32 OfferIndexOffset = (CurrentRequestIndex * MaxOffersPerRequest);
			for (int32 OfferIndex = 0; OfferIndex < ExpectedNumProducts; ++OfferIndex)
			{
				check(RequestedOfferIds.IsValidIndex(OfferIndex + OfferIndexOffset));
				const FString& OfferId = RequestedOfferIds[OfferIndex + OfferIndexOffset];
				if (!FoundOfferIds.Contains(OfferId))
				{
					UE_LOG_ONLINE_STOREV2(Warning, TEXT("  Offer [%s] was not returned"), *OfferId);
				}
			}
		}

		++CurrentRequestIndex;
		if (Requests.IsValidIndex(CurrentRequestIndex))
		{
			int32 ResetRet = Response.reset();
			StartNextRequest();
			return EStoreTaskCompletionStatus::Pending;
		}
		else
		{
			return EStoreTaskCompletionStatus::CompletedSuccessfully;
		}
	}

	static FText ConvertSonyStoreText(const char* Utf8StoreTextWithHtml)
	{
		FString StoreTextWithHtml = UTF8_TO_TCHAR(Utf8StoreTextWithHtml);
		FString StoreText;
		int32 TagStart = -1, EscapeStart = -1;
		int32 Index = 0;
		for (TCHAR Char : StoreTextWithHtml.GetCharArray())
		{
			// see if we're outside of an HTML tag
			if (TagStart >= 0)
			{
				// we're inside a tag, look for the end
				if (Char == '>') 
				{
					const FString HtmlTag = StoreTextWithHtml.Mid(TagStart + 1, Index - TagStart - 1);
					if (HtmlTag == TEXT("br"))
					{
						StoreText.AppendChar(TEXT('\n'));
					}

					// TODO: do we want to parse any more tags?
					TagStart = -1;
				}
			}
			else if (EscapeStart >= 0)
			{
				if (Char == ';')
				{
					FString Sequence = StoreTextWithHtml.Mid(EscapeStart + 1, Index - EscapeStart - 1);
					if (!Sequence.IsEmpty())
					{
						if (Sequence == TEXT("nbsp"))
						{
							StoreText.AppendChar(TEXT(' '));
						}
						else if (Sequence == TEXT("amp"))
						{
							StoreText.AppendChar(TEXT('&'));
						}
						else if (Sequence == TEXT("quot"))
						{
							StoreText.AppendChar(TEXT('"'));
						}
						else if (Sequence == TEXT("lt"))
						{
							StoreText.AppendChar(TEXT('<'));
						}
						else if (Sequence == TEXT("gt"))
						{
							StoreText.AppendChar(TEXT('>'));
						}
					}

					// end escape mode
					EscapeStart = -1;
				}
				else if (Index - EscapeStart >= 5)
				{
					// never saw a ;, just abort the escape
					for (int32 i = EscapeStart; i <= Index; ++i)
					{
						StoreText.AppendChar(StoreTextWithHtml[i]);
					}
					EscapeStart = -1;
				}
			}
			else
			{
				if (Char == '<')
				{
					TagStart = Index;
				}
				else if (Char == '&')
				{
					EscapeStart = Index;
				}
				else if (Char == '\n' || Char == '\r')
				{
					// Skip literal newlines, must use <br> instead
				}
				else
				{
					StoreText.AppendChar(Char);
				}
			}
			++Index;
		}
		return FText::FromString(StoreText);
	}

	virtual void Finalize() override
	{
		TArray<FUniqueOfferId> OfferIds;
		if (bWasSuccessful)
		{
			for (auto const& Offer : OffersList)
			{
				OfferIds.Add(Offer->OfferId);
				OnlineStore->CachedOffers.Add(Offer->OfferId, Offer);
			}
		}
		Delegate.ExecuteIfBound(bWasSuccessful, OfferIds, ErrorString);
	}

	int32 CurrentRequestIndex = 0;
	TArray<NpToolkit::Commerce::Request::GetProducts> Requests;

	FOnQueryOnlineStoreOffersComplete Delegate;
	TArray<FOnlineStoreOfferRef> OffersList;
	TArray<FUniqueOfferId> RequestedOfferIds;
};

void FOnlineStorePS4::QueryOffersById(const FUniqueNetId& UserId, const TArray<FUniqueOfferId>& OfferIds, const FOnQueryOnlineStoreOffersComplete& Delegate)
{
	QueueStoreTask(new FGetProductInfoListOp(FUniqueNetIdPS4::Cast(UserId), OfferIds, Delegate));
}

void FOnlineStorePS4::QueryOffersByFilter(const FUniqueNetId& UserId, const FOnlineStoreFilter& Filter, const FOnQueryOnlineStoreOffersComplete& Delegate)
{
	// TODO: write me
	Ps4Subsystem->ExecuteNextTick([Delegate]() {
		Delegate.ExecuteIfBound(false, TArray<FUniqueOfferId>(), TEXT("not yet implemented"));
	});
}

void FOnlineStorePS4::GetCategories(TArray<FOnlineStoreCategory>& OutCategories) const
{
	OutCategories = CachedCategories;
}

void FOnlineStorePS4::GetOffers(TArray<FOnlineStoreOfferRef>& OutOffers) const
{
	CachedOffers.GenerateValueArray(OutOffers);
}

TSharedPtr<FOnlineStoreOffer> FOnlineStorePS4::GetOffer(const FUniqueOfferId& OfferId) const
{
	const FOnlineStoreOfferRef* EntryPtr = CachedOffers.Find(OfferId);
	if (EntryPtr == nullptr)
	{
		return TSharedPtr<FOnlineStoreOffer>();
	}
	return *EntryPtr;
}
