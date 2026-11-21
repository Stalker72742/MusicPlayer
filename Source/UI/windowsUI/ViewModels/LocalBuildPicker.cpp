#include "LocalBuildPicker.h"

#include <QDir>
#include <QFileInfo>
#include <QWindow>

#ifdef Q_OS_WIN

#include <iterator>
#include <string>

#include <windows.h>
#include <shobjidl.h>

namespace LocalBuildPickerDetail
{
    constexpr DWORD UseFolderButtonId = 1;

    QString ItemPath(IShellItem* item)
    {
        if (!item)
            return {};

        PWSTR path = nullptr;
        if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) || !path)
            return {};

        const QString result = QDir::fromNativeSeparators(QString::fromWCharArray(path));
        CoTaskMemFree(path);
        return result;
    }

    bool IsFolder(IShellItem* item)
    {
        SFGAOF attributes = 0;
        return item && SUCCEEDED(item->GetAttributes(SFGAO_FOLDER | SFGAO_FILESYSTEM, &attributes))
            && (attributes & SFGAO_FOLDER) && (attributes & SFGAO_FILESYSTEM);
    }

    /// Takes the folder when "Use this folder" is pressed and closes the dialog.
    class DialogEvents final : public IFileDialogEvents, public IFileDialogControlEvents
    {
    public:
        QString chosenFolder;

        // IUnknown
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override
        {
            if (!object)
                return E_POINTER;

            if (riid == __uuidof(IUnknown) || riid == __uuidof(IFileDialogEvents))
                *object = static_cast<IFileDialogEvents*>(this);
            else if (riid == __uuidof(IFileDialogControlEvents))
                *object = static_cast<IFileDialogControlEvents*>(this);
            else
            {
                *object = nullptr;
                return E_NOINTERFACE;
            }
            AddRef();
            return S_OK;
        }
        ULONG STDMETHODCALLTYPE AddRef() override { return ++refCount; }
        ULONG STDMETHODCALLTYPE Release() override
        {
            const ULONG count = --refCount;
            if (count == 0)
                delete this;
            return count;
        }

        // IFileDialogEvents
        HRESULT STDMETHODCALLTYPE OnFileOk(IFileDialog*) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE OnFolderChanging(IFileDialog*, IShellItem*) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE OnFolderChange(IFileDialog*) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE OnSelectionChange(IFileDialog*) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE OnShareViolation(IFileDialog*, IShellItem*, FDE_SHAREVIOLATION_RESPONSE*) override
        {
            return E_NOTIMPL;
        }
        HRESULT STDMETHODCALLTYPE OnTypeChange(IFileDialog*) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE OnOverwrite(IFileDialog*, IShellItem*, FDE_OVERWRITE_RESPONSE*) override
        {
            return E_NOTIMPL;
        }

        // IFileDialogControlEvents
        HRESULT STDMETHODCALLTYPE OnItemSelected(IFileDialogCustomize*, DWORD, DWORD) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE OnCheckButtonToggled(IFileDialogCustomize*, DWORD, BOOL) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE OnControlActivating(IFileDialogCustomize*, DWORD) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE OnButtonClicked(IFileDialogCustomize* customize, DWORD id) override
        {
            if (id != UseFolderButtonId || !customize)
                return S_OK;

            IFileDialog* dialog = nullptr;
            if (FAILED(customize->QueryInterface(__uuidof(IFileDialog), reinterpret_cast<void**>(&dialog))))
                return S_OK;

            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetCurrentSelection(&item)) && IsFolder(item))
                chosenFolder = ItemPath(item);
            if (item)
                item->Release();

            if (chosenFolder.isEmpty())
            {
                item = nullptr;
                if (SUCCEEDED(dialog->GetFolder(&item)))
                    chosenFolder = ItemPath(item);
                if (item)
                    item->Release();
            }

            if (!chosenFolder.isEmpty())
                dialog->Close(S_FALSE);
            dialog->Release();
            return S_OK;
        }

    private:
        ULONG refCount = 1;
    };

    QString PickNative(QWindow* owner, const QString& appName)
    {
        const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool bUninitialize = SUCCEEDED(comResult);

        QString result;
        IFileOpenDialog* dialog = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))))
        {
            const std::wstring title = QStringLiteral("Choose a %1 package or a deployed folder").arg(appName).toStdWString();
            const std::wstring filterName = QStringLiteral("%1 build (*.zip, package.json)").arg(appName).toStdWString();
            const COMDLG_FILTERSPEC filters[] = {
                {filterName.c_str(), L"*.zip;package.json"},
                {L"All files (*.*)", L"*.*"},
            };

            FILEOPENDIALOGOPTIONS options = 0;
            dialog->GetOptions(&options);
            dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);
            dialog->SetTitle(title.c_str());
            dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
            dialog->SetFileTypeIndex(1);

            auto* events = new DialogEvents;
            DWORD cookie = 0;
            const bool bAdvised = SUCCEEDED(dialog->Advise(static_cast<IFileDialogEvents*>(events), &cookie));

            IFileDialogCustomize* customize = nullptr;
            if (SUCCEEDED(dialog->QueryInterface(__uuidof(IFileDialogCustomize), reinterpret_cast<void**>(&customize))))
            {
                customize->AddPushButton(UseFolderButtonId, L"Use this folder");
                customize->Release();
            }

            const HWND ownerHwnd = owner ? reinterpret_cast<HWND>(owner->winId()) : nullptr;
            const HRESULT shown = dialog->Show(ownerHwnd);
            if (!events->chosenFolder.isEmpty())
                result = events->chosenFolder;
            else if (SUCCEEDED(shown) && shown != S_FALSE)
            {
                IShellItem* item = nullptr;
                if (SUCCEEDED(dialog->GetResult(&item)))
                    result = ItemPath(item);
                if (item)
                    item->Release();
            }

            if (bAdvised)
                dialog->Unadvise(cookie);
            events->Release();
            dialog->Release();
        }

        if (bUninitialize)
            CoUninitialize();
        return result;
    }
}

#else

#include <QFileDialog>

#endif

QString LocalBuildPicker::Pick(QWindow* owner, const QString& appName)
{
#ifdef Q_OS_WIN
    QString path = LocalBuildPickerDetail::PickNative(owner, appName);
#else
    Q_UNUSED(owner);
    QString path = QFileDialog::getOpenFileName(nullptr, QStringLiteral("Choose a %1 package").arg(appName), {},
        QStringLiteral("%1 build (*.zip package.json)").arg(appName));
#endif
    if (path.isEmpty())
        return {};

    const QFileInfo info(path);
    if (info.isFile() && info.fileName().compare(QStringLiteral("package.json"), Qt::CaseInsensitive) == 0)
        return info.absolutePath();
    return path;
}
