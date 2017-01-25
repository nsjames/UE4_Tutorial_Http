# UE4 Tutorial - Http
---
Http requests in UE4 are fairly straight forward, however here are a few pitfalls and suggestions:
  - **UE4 only accepts GET/PUT/POST requests.** This means you can not send DELETE/PATCH requests.
  - You can write your own serializers/deserializers, but I highly recommend the use of **`USTRUCT()`** and **`FJsonObjectConverter`**
  - I find it easiest to use the service as an Actor child. You should spawn it into the level once and reference it.




#### Setup
---
**Before you start** make sure you have included the required dependencies.

```
# Path: Source/YourProject/YourProject.Build.cs
PrivateDependencyModuleNames.AddRange(new string[] { "Http", "Json", "JsonUtilities" });
```
 - **`Http`** is our trusty ue4 http implementation.
 - **`Json`** is the json conversion library
 - **`JsonUtilities`** has the `FJsonObjectConverter` we will be using to convert Json data to Struct data

#
#
#
#
#
---
> **At this point you could probably just grab the files and go over them, but here's a detailed explanation as well as some reasoning behind design decisions and a friendly reminder to keep code clean.**
---

#
#
#
#
#
# Table Of Contents
  - [**USTRUCTS()**](#ustructs)
  - [**Some Important Variables**](#some-important-variables)
  - [**Internal Methods**](#internal-methods)
  - [**Sending a Request**](#sending-a-request)
  - [**Checking for valid Responses**](#checking-for-valid-responses)
  - [**Serialization and Deserialization**](#serialization-and-deserialization)
  - [**Real World Example**](#okay-lets-look-at-some-real-world-http-requests)
  
#
#
#
#
#

---
### USTRUCTS()
`FRequest_Login` and `FResponse_Login` are both used to pass data back and forth between `UE4` and your `Back-End Server`.
I wont be touching on back-end servers but I will be showing the `JSON` that will be sent and returned.

#
#

**`FRequest_Login`** holds the `email` and `password` that we are using to log into our account.
```
USTRUCT()
struct FRequest_Login {
	GENERATED_BODY()
	UPROPERTY() FString email;
	UPROPERTY() FString password;

	FRequest_Login() {}
};
```
**`JSON EXAMPLE: { "email":"some@email.com", "password":"strongpassword" }`**

#
#

**`FResponse_Login`** holds the returned response from the login request. 
```
USTRUCT()
struct FResponse_Login {
	GENERATED_BODY()
	UPROPERTY() int id;
	UPROPERTY() FString name;
	UPROPERTY() FString hash;

	FResponse_Login() {}
};
```
**`JSON EXAMPLE: { "id":1, "name":"Batman", "hash":"asdf-qwer-dfgh-erty" }`**

#
#

> **Note:** *You should provide a 'hash' property on every player login success. It will be used to verify their account on every subsequent request; since APIs dont hold state.*

#
#
#
#
#
---
### Some important variables:
  - `FHttpModule* Http;` - Holds a reference to UE4's Http implementation. It's used to create request objects. 
  - `FString ApiBaseUrl` - **You should replace this with your actual API url.**
  - `FString AuthorizationHeader` - This is the `key` for the Authentication header. Your back-end might expect a different form of this such as `X-Auth-Token`, `X-Requested-With` or something similar.

#
#
#
#
#
---
### Internal Methods:
These are just some methods that you can use to build eloquently written api calls.

#
#

```
    TSharedRef<IHttpRequest> RequestWithRoute(FString Subroute);
	void SetRequestHeaders(TSharedRef<IHttpRequest>& Request);
```
Both `RequestWithRoute` and `SetRequestHeaders` are used to initialize `Http Request Objects`. 
**They shouldn't be called directly, only through the methods below.**

#
#

```
	TSharedRef<IHttpRequest> GetRequest(FString Subroute);
	TSharedRef<IHttpRequest> PostRequest(FString Subroute, FString ContentJsonString);
```
`GetRequest` and `PostRequest` are the proper methods to call. 
*I intentionally left out `PutRequest` so that you may implement it using the same structure.*

> *You might be asking **Why not just have one method that accepts a Verb?** - The simple answer is that inserting a string into a parameter is not only sloppy, but will also add error checking and useless complexity to a very simple method.* 


Let's take a look at the implementations of **`PostRequest`**, **`RequestWithRoute`** and **`SetRequestHeaders`**
```
TSharedRef<IHttpRequest> AHttpService::PostRequest(FString Subroute, FString ContentJsonString) {
	TSharedRef<IHttpRequest> Request = RequestWithRoute(Subroute);
	Request->SetVerb("POST");
	Request->SetContentAsString(ContentJsonString);
	return Request;
}

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
```
As you can see, **`PostRequest`** is very simple and uses **`RequestWithRoute`** to build itself, keeping everything nice and clean. 
The flow for **`PostRequest`** goes as follows:
  - Get `Request Object` with a `subroute` and set it's `Headers`
  - Set the Verb to **`POST`**
  - Set the `RequestObjects`'s Content to a `Json formatted string`
  - Return the `RequestObject`
  
#
#
#
#
#
---

### Sending a Request:
```
    void Send(TSharedRef<IHttpRequest>& Request);
```
This is really just a semantically named method for cleanliness. As you can see from the code below it really doesn't do much besides clean up the naming conventions and make the code more readable.
```
void AHttpService::Send(TSharedRef<IHttpRequest>& Request) {
	Request->ProcessRequest();
}
```
#
#
#
#
#
---
### Checking for valid Responses:
**`ResponseIsValid`** is used to deeply check if a response is valid.
  - `!bWasSuccessful` is returned from the Http request made by UE4. It's the first check because if it fails no further information will be given.
  - `!Response.IsValid()` is also returned from the UE4 request, and means that most likely the request succeeded, but the response can't be parsed.
  - If the `ResponseCode` is not **`Ok` ( 200 )** then we will also return false, as well as log out the code returned.
```
bool AHttpService::ResponseIsValid(FHttpResponsePtr Response, bool bWasSuccessful) {
	if (!bWasSuccessful || !Response.IsValid()) return false;
	if (EHttpResponseCodes::IsOk(Response->GetResponseCode())) return true;
	else {
		UE_LOG(LogTemp, Warning, TEXT("Http Response returned error code: %d"), Response->GetResponseCode());
		return false;
	}
}
```
#
#
#
#
#
---
### Serialization and Deserialization
We're going to be using **`FJsonObjectConverter`** to convert json to scructs and structs to json.
Like I said above I suggest you use `FJsonObjectConverter`.
Here are some reasons why:
  - Support for TArray, even TArray<NestedStruct>
  - Support for Enum from string/int ( for example a json of { "itemType":"Weapon" } will become `EItemType itemType = EItemType::Weapon`;
  - Direct conversion into a USTRUCT() which is garbage collected.

#
#

Let's look at the two methods that handle serialization and deserialization.
##### Get Json String From Struct:
#
```
template <typename StructType>
void AHttpService::GetJsonStringFromStruct(StructType FilledStruct, FString& StringOutput) {
	FJsonObjectConverter::UStructToJsonObjectString(StructType::StaticStruct(), &FilledStruct, StringOutput, 0, 0);
}
```
This method gets a Json Formatted String from a struct of type and binds it to the `StringOutput`.
We see this in action in the [`Login()`](#for-the-login-request) method.

##### Get Struct From Json String:
#
```
template <typename StructType>
void AHttpService::GetStructFromJsonString(FHttpResponsePtr Response, StructType& StructOutput) {
	StructType StructData;
	FString JsonString = Response->GetContentAsString();
	FJsonObjectConverter::JsonObjectStringToUStruct<StructType>(JsonString, &StructOutput, 0, 0);
}
```
This method does the exact opposite of `GetJsonStringFromStruct()` and binds a Struct using the Json Formatted String to the `StructOutput`.
We see this in action in the [`LoginResponse()`](#for-the-login-response) method.

#
#
#
#
#
---
### Okay! Let's look at some real world Http Requests!
I've included a simple login example in the file as well just to illustrate how this all comes together nicely and neatly.

#
#


### For the Login (Request)
```
void AHttpService::Login(FRequest_Login LoginCredentials) {
	FString ContentJsonString;
	GetJsonStringFromStruct<FRequest_Login>(LoginCredentials, ContentJsonString);

	TSharedRef<IHttpRequest> Request = PostRequest("user/login", ContentJsonString);
	Request->OnProcessRequestComplete().BindUObject(this, &AHttpService::LoginResponse);
	Send(Request);
}
```
  - `FString ContentJsonString` holds the returned json from the **`GetJsonStringFromStruct`** method.
  - `TSharedRef<IHttpRequest> Request` holds the RequestObject that we get from the **`PostRequest`** method. Note that we are passing in a subroute relative to the `ApiBaseUrl` we put into the **`.h`** file.
  - **`Request->OnProcessRequestComplete().BindUObject(this, &AHttpService::LoginResponse);`** is highly important here. We use this to bind the Resquest's response to a method.
  - **`Send(Request);`** does just what it says. Hence the nicely semantic naming convention.

### Here's an example of the method being called:
```
CALLED FROM BeginPlay():
    FRequest_Login LoginCredentials;
	LoginCredentials.email = TEXT("asdf@asdf.com");
	LoginCredentials.password = TEXT("asdfasdf");
	Login(LoginCredentials);
```

#
#

### For the Login (Response)
```
void AHttpService::LoginResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
	if (!ResponseIsValid(Response, bWasSuccessful)) return;

	FResponse_Login LoginResponse;
	GetStructFromJsonString<FResponse_Login>(Response, LoginResponse);

	SetAuthorizationHash(LoginResponse.hash);

	UE_LOG(LogTemp, Warning, TEXT("Id is: %d"), LoginResponse.id);
	UE_LOG(LogTemp, Warning, TEXT("Name is: %s"), *LoginResponse.name);
}
```
Let's take a moment to look at the **Mandatory** parameters.
  - `FHttpRequestPtr Request`
  - `FHttpResponsePtr Response`
  - `bool bWasSuccessful` - If the response was successful at all. ( Will fail for instance if the server is down. )
  
You don't have to worry about passing these in, they are passed automatically by UE4. 

```
    if (!ResponseIsValid(Response, bWasSuccessful)) return;
```
We don't want to continue if the response is bad, so it's good practice to run this method.

```
    FResponse_Login LoginResponse;
    GetStructFromJsonString<FResponse_Login>(Response, LoginResponse);
```
Here we're binding the Json Response into our custom `FResponse_Login` Struct.
> **Note**
> A struct will simply not fill itself out if there are properties missing. Keep that in mind.

#
```
    SetAuthorizationHash(LoginResponse.hash);
```
We can set the hash for further requests here. Really though, we should be passing it back to the specific **Player** and bind it on each request, otherwise **every player will be the same user**.



#
#
#
#
#



> **Here's something nifty.**
> You can pass in more parameters by adding them to the **Response Method**, and then binding them to the request like so:
> `BindUObject(this, &AHttpService::LoginResponse, SomeCustomPlayerState);`
> Which will allow you to later bind things back by, for instance, calling a method like so:
> `SomeCustomPlayerState->PlayerLoggedIn(LoginResponse);`
> This gives you a flow similar to a `Promise` ( `doApiCall.then()` ) in javascript.
  



