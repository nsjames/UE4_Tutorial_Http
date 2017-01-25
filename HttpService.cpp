// Fill out your copyright notice in the Description page of Project Settings.

#include "tester2.h"
#include "HttpService.h"



AHttpService::AHttpService(){ PrimaryActorTick.bCanEverTick = false; }
void AHttpService::BeginPlay() { 
	Super::BeginPlay(); 

	// We don't want clients to be able to run HTTP requests. Only servers.
	if (!HasAuthority()) return;
	Http = &FHttpModule::Get(); 
	
	FRequest_Login LoginCredentials;
	LoginCredentials.email = TEXT("asdf@asdf.com");
	LoginCredentials.password = TEXT("asdfasdf");
	Login(LoginCredentials);
}




/**********************************************************************************************************************************************/




TSharedRef<IHttpRequest> AHttpService::RequestWithRoute(FString Subroute) {
	TSharedRef<IHttpRequest> Request = Http->CreateRequest();
	Request->SetURL(ApiBaseUrl + Subroute);
	SetRequestHeaders(Request);
	return Request;
}

void AHttpService::SetRequestHeaders(TSharedRef<IHttpRequest>& Request) {
	Request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accepts"), TEXT("application/json"));
	Request->SetHeader(AuthorizationHeader, AuthorizationHash);
}

TSharedRef<IHttpRequest> AHttpService::GetRequest(FString Subroute) {
	TSharedRef<IHttpRequest> Request = RequestWithRoute(Subroute);
	Request->SetVerb("GET");
	return Request;
}

TSharedRef<IHttpRequest> AHttpService::PostRequest(FString Subroute, FString ContentJsonString) {
	TSharedRef<IHttpRequest> Request = RequestWithRoute(Subroute);
	Request->SetVerb("POST");
	Request->SetContentAsString(ContentJsonString);
	return Request;
}

void AHttpService::Send(TSharedRef<IHttpRequest>& Request) {
	Request->ProcessRequest();
}

bool AHttpService::ResponseIsValid(FHttpResponsePtr Response, bool bWasSuccessful) {
	if (!bWasSuccessful || !Response.IsValid()) return false;
	if (EHttpResponseCodes::IsOk(Response->GetResponseCode())) return true;
	else {
		UE_LOG(LogTemp, Warning, TEXT("Http Response returned error code: %d"), Response->GetResponseCode());
		return false;
	}
}

void AHttpService::SetAuthorizationHash(FString Hash) {
	AuthorizationHash = Hash;
}



/**********************************************************************************************************************************************/



template <typename StructType>
void AHttpService::GetJsonStringFromStruct(UScriptStruct* StaticStruct, StructType FilledStruct, FString& StringOutput) {
	FJsonObjectConverter::UStructToJsonObjectString(StaticStruct, &FilledStruct, StringOutput, 0, 0);
}

template <typename StructType>
void AHttpService::GetStructFromJsonString(FHttpResponsePtr Response, StructType& StructOutput) {
	StructType StructData;
	FString JsonString = Response->GetContentAsString();
	FJsonObjectConverter::JsonObjectStringToUStruct<StructType>(JsonString, &StructOutput, 0, 0);
}



/**********************************************************************************************************************************************/



void AHttpService::Login(FRequest_Login LoginCredentials) {
	FString ContentJsonString;
	GetJsonStringFromStruct<FRequest_Login>(FRequest_Login::StaticStruct(), LoginCredentials, ContentJsonString);

	TSharedRef<IHttpRequest> Request = PostRequest("user/login", ContentJsonString);
	Request->OnProcessRequestComplete().BindUObject(this, &AHttpService::LoginResponse);
	Send(Request);
}

void AHttpService::LoginResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
	if (!ResponseIsValid(Response, bWasSuccessful)) return;

	FResponse_Login LoginResponse;
	GetStructFromJsonString<FResponse_Login>(Response, LoginResponse);

	SetAuthorizationHash(LoginResponse.hash);

	UE_LOG(LogTemp, Warning, TEXT("Id is: %d"), LoginResponse.id);
	UE_LOG(LogTemp, Warning, TEXT("Name is: %s"), *LoginResponse.name);
}

