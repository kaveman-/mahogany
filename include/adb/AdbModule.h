///////////////////////////////////////////////////////////////////////////////
// Project:     M
// File name:   adb/AdbModule.h - base class for ADB importers and exporters
// Purpose:     this class makes it possible to dynamically load ADB importers
//              or exporters from M modules, it provides functions to enum all
//              available importers/exporters and to find one by name
// Author:      Vadim Zeitlin
// Modified by:
// Created:     19.05.00 (extracted from AdbImport.h)
// CVS-ID:      $Id$
// Copyright:   (c) 2000 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     M license
///////////////////////////////////////////////////////////////////////////////

#ifndef _ADB_MODULE_H_
#define _ADB_MODULE_H_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// undef this to statically link all ADB importers into the executable
#define USE_ADB_MODULES

// include the base class declaration
#ifdef USE_ADB_MODULES
   #include "MModule.h"

   typedef MModule AdbModuleBase;
#else // !USE_ADB_MODULES
   #include "MObject.h"

   typedef MObjectRC AdbModuleBase;
#endif // USE_ADB_MODULES/!USE_ADB_MODULES

// ----------------------------------------------------------------------------
// AdbModule allows loading the ADB importers/exporters during run-time. The
// modules can be enumerated or accessed by name if the exact name is known.
//
// An ADB module class must use DECLARE_ADB_MODULE() in its class declaration
// and IMPLEMENT_ADB_MODULE() in the implementation file.
// ----------------------------------------------------------------------------

class AdbModule : public AdbModuleBase
{
public:
   // dynamic creation helpers
#ifndef USE_ADB_MODULES
   typedef AdbModule *(*Constructor)();
#endif // !USE_ADB_MODULES

   // list holding information about all ADB importers we have
   struct AdbModuleInfo
   {
      String name;    // internal name
      String desc;    // descriptive name (shown to the user)

      AdbModuleInfo *next;  // next node in the linked list

#ifdef USE_ADB_MODULES
      AdbModule *CreateModule() const;

      AdbModuleInfo(const char *name, const char *desc);
#else // !USE_ADB_MODULES
      Constructor CreateModule;   // creator function

      // ctor for the struct
      AdbModuleInfo(const char *name, Constructor ctor, const char *desc);
#endif // USE_ADB_MODULES/!USE_ADB_MODULES
   };

   // access the linked list of AdbModuleInfo, you must call FreeAdbModuleInfo()
   // after you have used the list
   static AdbModuleInfo *GetAdbModuleInfo(const char *kind);
   static void FreeAdbModuleInfo(AdbModuleInfo *info);

   // enumerate all available importers: return the names (to make it possible
   // to create import objects) and the descriptions (to show to the user);
   // returns the number of different importers
   static size_t EnumModules(const char *kind,
                             wxArrayString& names, wxArrayString& descs);

   // get importer by name (it's a new pointer, caller must DecRef() it)
   static AdbModule *GetModuleByName(const char *kind, const String& name);

   // get the name and description (shown to the user) of the format imported
   // by this class (these functions are automatically generated during
   // IMPLEMENT_ADB_MODULE macro expansion
   virtual const char *GetName() const = 0;
   virtual const char *GetFormatDesc() const = 0;
   virtual const char *GetDescription() const = 0;

   virtual ~AdbModule() { }

private:
   friend struct AdbModuleInfo; // give it access to ms_listModules
   static AdbModuleInfo *ms_listModules;
};

// ----------------------------------------------------------------------------
// dynamic object creation macros
// ----------------------------------------------------------------------------

#ifdef USE_ADB_MODULES

// note that GetName() and GetDescription() declarations are inside
// MMODULE_DEFINE macro
#define DECLARE_ADB_MODULE()                                               \
   virtual const char *GetFormatDesc() const;                              \
   DEFAULT_ENTRY_FUNC                                                      \
   MMODULE_DEFINE()

// parameters of this macro are:
//    modint - the interface of the module (AdbImporter or AdbExporter)
//    cname  - the name of the class (derived from AdbModule)
//    desc   - the short description shown in the modules dialog
//    format - the ADB format shown in the ADB dialogs
//    author - the module author/copyright string
#define IMPLEMENT_ADB_MODULE(modint, cname, desc, format, Author)          \
   MMODULE_BEGIN_IMPLEMENT(cname, #cname, modint, "", "1.00")              \
      MMODULE_PROP("author", Author)                                       \
      MMODULE_PROP("adbformat", format)                                    \
   MMODULE_END_IMPLEMENT(cname)                                            \
   const char *cname::GetFormatDesc() const                                \
   {                                                                       \
      return GetMModuleProperty(ms_properties, "adbformat");               \
   }                                                                       \
   MModule *cname::Init(int /* version_major */,                           \
                        int /* version_minor */,                           \
                        int /* version_release */,                         \
                        MInterface * /* minterface */,                     \
                        int * /* errorCode */)                             \
   {                                                                       \
      return new cname();                                                  \
   }

#else // !USE_ADB_MODULES

#define DECLARE_ADB_MODULE()                                               \
   const char *GetName() const;                                            \
   const char *GetFormatDesc() const;                                      \
   const char *GetDescription() const;                                     \
   static AdbModuleInfo ms_info

#define IMPLEMENT_ADB_MODULE(modint, name, desc, format, author)           \
   const char *name::GetName() const { return #name; }                     \
   const char *name::GetFormatDesc() const { return _(format); }           \
   const char *name::GetDescription() const { return _(desc); }            \
   AdbModule *ConstructorFor##name() { return new name; }                  \
   AdbModule::AdbModuleInfo                                                \
      name::ms_info(#name, ConstructorFor##name, desc)

#endif // USE_ADB_MODULES/!USE_ADB_MODULES

#endif // _ADB_MODULE_H_
