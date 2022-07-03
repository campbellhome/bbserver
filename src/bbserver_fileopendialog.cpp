// Downloaded from https://github.com/pauldotknopf/WindowsSDK7-Samples/blob/master/winui/shell/appplatform/commonfiledialog/CommonFileDialogApp.cpp
// because microsoft links have gone dead, as always
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//

#include "sb.h"
BB_WARNING_PUSH(4577 4668 4820 4946)
#define WIN32_LEAN_AND_MEAN
#include <windows.h> // For common windows data types and function headers
#define STRICT_TYPED_ITEMIDS
#include <knownfolders.h> // for KnownFolder APIs/datatypes/function headers
#include <new>
#include <objbase.h>     // For COM headers
#include <propidl.h>     // for the Property System APIs
#include <propkey.h>     // for the Property key APIs/datatypes
#include <propvarutil.h> // for PROPVAR-related functions
#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h> // for IFileDialogEvents and IFileDialogControlEvents
#include <shtypes.h>  // for COMDLG_FILTERSPEC
#include <strsafe.h>  // for StringCchPrintfW
BB_WARNING_POP

#pragma comment(linker, "\"/manifestdependency:type='Win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Propsys.lib")

static bool s_bInitialized = false;
extern "C" void FileOpenDialog_Init(void)
{
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	s_bInitialized = SUCCEEDED(hr);
}

extern "C" void FileOpenDialog_Shutdown(void)
{
	if(s_bInitialized) {
		CoUninitialize();
	}
}

const COMDLG_FILTERSPEC c_rgSaveTypes[] =
    {
	    { L"Blackbox Logs (*.bbox)", L"*.bbox" },
	    { L"Text Logs (*.txt; *.log)", L"*.txt;*.log" },
	    { L"All Documents (*.*)", L"*.*" }
    };

// Indices of file types
#define INDEX_BLACKBOX 1
#define EXTENSION_BLACKBOX L"bbox"

/* File Dialog Event Handler *****************************************************************************************************/

class CDialogEventHandler : public IFileDialogEvents,
                            public IFileDialogControlEvents
{
public:
	// IUnknown methods
	IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv)
	{
		static const QITAB qit[] = {
			QITABENT(CDialogEventHandler, IFileDialogEvents),
			QITABENT(CDialogEventHandler, IFileDialogControlEvents),
			{ 0 },
		};
		return QISearch(this, qit, riid, ppv);
	}

	IFACEMETHODIMP_(ULONG)
	AddRef()
	{
		return (ULONG)InterlockedIncrement(&_cRef);
	}

	IFACEMETHODIMP_(ULONG)
	Release()
	{
		long cRef = InterlockedDecrement(&_cRef);
		if(!cRef)
			delete this;
		return (ULONG)cRef;
	}

	// IFileDialogEvents methods
	IFACEMETHODIMP OnFileOk(IFileDialog *) { return S_OK; }
	IFACEMETHODIMP OnFolderChange(IFileDialog *) { return S_OK; }
	IFACEMETHODIMP OnFolderChanging(IFileDialog *, IShellItem *) { return S_OK; }
	IFACEMETHODIMP OnHelp(IFileDialog *) { return S_OK; }
	IFACEMETHODIMP OnSelectionChange(IFileDialog *) { return S_OK; }
	IFACEMETHODIMP OnShareViolation(IFileDialog *, IShellItem *, FDE_SHAREVIOLATION_RESPONSE *) { return S_OK; }
	IFACEMETHODIMP OnTypeChange(IFileDialog *) { return S_OK; }
	IFACEMETHODIMP OnOverwrite(IFileDialog *, IShellItem *, FDE_OVERWRITE_RESPONSE *) { return S_OK; }

	// IFileDialogControlEvents methods
	IFACEMETHODIMP OnItemSelected(IFileDialogCustomize *, DWORD, DWORD) { return S_OK; }
	IFACEMETHODIMP OnButtonClicked(IFileDialogCustomize *, DWORD) { return S_OK; }
	IFACEMETHODIMP OnCheckButtonToggled(IFileDialogCustomize *, DWORD, BOOL) { return S_OK; }
	IFACEMETHODIMP OnControlActivating(IFileDialogCustomize *, DWORD) { return S_OK; }

private:
	virtual ~CDialogEventHandler() {}
	long _cRef = 1;
	u8 pad[4] = {};
};

// Instance creation helper
HRESULT CDialogEventHandler_CreateInstance(REFIID riid, void **ppv)
{
	*ppv = NULL;
	CDialogEventHandler *pDialogEventHandler = new(std::nothrow) CDialogEventHandler();
	HRESULT hr = pDialogEventHandler ? S_OK : E_OUTOFMEMORY;
	if(SUCCEEDED(hr)) {
		hr = pDialogEventHandler->QueryInterface(riid, ppv);
		pDialogEventHandler->Release();
	}
	return hr;
}

/* Common File Dialog Snippets ***************************************************************************************************/

// This code snippet demonstrates how to work with the common file dialog interface
extern "C" sb_t FileOpenDialog_Show(void)
{
	sb_t result = { BB_EMPTY_INITIALIZER };
	if(!s_bInitialized)
		return result;

	// CoCreate the File Open Dialog object.
	IFileDialog *pfd = NULL;
	HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
	if(SUCCEEDED(hr)) {
		// Create an event handling object, and hook it up to the dialog.
		IFileDialogEvents *pfde = NULL;
		hr = CDialogEventHandler_CreateInstance(IID_PPV_ARGS(&pfde));
		if(SUCCEEDED(hr)) {
			// Hook up the event handler.
			DWORD dwCookie;
			hr = pfd->Advise(pfde, &dwCookie);
			if(SUCCEEDED(hr)) {
				// Set the options on the dialog.
				DWORD dwFlags;

				// Before setting, always get the options first in order not to override existing options.
				hr = pfd->GetOptions(&dwFlags);
				if(SUCCEEDED(hr)) {
					// In this case, get shell items only for file system items.
					hr = pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM);
					if(SUCCEEDED(hr)) {
						// Set the file types to display only. Notice that, this is a 1-based array.
						hr = pfd->SetFileTypes(ARRAYSIZE(c_rgSaveTypes), c_rgSaveTypes);
						if(SUCCEEDED(hr)) {
							// Set the selected file type index to Word Docs for this example.
							hr = pfd->SetFileTypeIndex(INDEX_BLACKBOX);
							if(SUCCEEDED(hr)) {
								// Set the default extension to be ".doc" file.
								hr = pfd->SetDefaultExtension(EXTENSION_BLACKBOX);
								if(SUCCEEDED(hr)) {
									// Show the dialog
									hr = pfd->Show(NULL);
									if(SUCCEEDED(hr)) {
										// Obtain the result, once the user clicks the 'Open' button.
										// The result is an IShellItem object.
										IShellItem *psiResult;
										hr = pfd->GetResult(&psiResult);
										if(SUCCEEDED(hr)) {
											// We are just going to print out the name of the file for sample sake.
											PWSTR pszFilePath = NULL;
											hr = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
											if(SUCCEEDED(hr)) {
												sb_reserve(&result, (u32)(wcslen(pszFilePath) * 2));
												if(result.data) {
													size_t numCharsConverted = 0;
													wcstombs_s(&numCharsConverted, result.data, result.allocated, pszFilePath, _TRUNCATE);
													result.count = (u32)strlen(result.data) + 1;
												}
												//result = sb_from_c_string(pszFilePath);
												CoTaskMemFree(pszFilePath);
											}
											psiResult->Release();
										}
									}
								}
							}
						}
					}
				}
				// Unhook the event handler.
				pfd->Unadvise(dwCookie);
			}
			pfde->Release();
		}
		pfd->Release();
	}
	return result;
}
