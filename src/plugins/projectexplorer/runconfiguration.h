/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#pragma once

#include "projectconfiguration.h"
#include "projectexplorerconstants.h"
#include "applicationlauncher.h"
#include "devicesupport/idevice.h"

#include <utils/port.h>
#include <utils/processhandle.h>
#include <utils/qtcassert.h>
#include <utils/icon.h>

#include <QPointer>
#include <QWidget>

#include <functional>
#include <memory>

namespace Utils { class OutputFormatter; }

namespace ProjectExplorer {
class Abi;
class BuildConfiguration;
class IRunConfigurationAspect;
class RunConfiguration;
class RunConfigWidget;
class RunControl;
class Target;

namespace Internal {
class RunControlPrivate;
class RunWorkerPrivate;
class SimpleRunControlPrivate;
} // Internal

/**
 * An interface for a hunk of global or per-project
 * configuration data.
 *
 */

class PROJECTEXPLORER_EXPORT ISettingsAspect : public QObject
{
    Q_OBJECT

public:
    ISettingsAspect() { }

    /// Create a configuration widget for this settings aspect.
    virtual QWidget *createConfigWidget(QWidget *parent) = 0;
    /// "Virtual default constructor"
    virtual ISettingsAspect *create() const = 0;
    /// "Virtual copy constructor"
    ISettingsAspect *clone() const;

protected:
    ///
    friend class IRunConfigurationAspect;
    /// Converts current object into map for storage.
    virtual void toMap(QVariantMap &map) const = 0;
    /// Read object state from @p map.
    virtual void fromMap(const QVariantMap &map) = 0;
};


/**
 * An interface to facilitate switching between hunks of
 * global and per-project configuration data.
 *
 */

class PROJECTEXPLORER_EXPORT IRunConfigurationAspect : public QObject
{
    Q_OBJECT

public:
    explicit IRunConfigurationAspect(RunConfiguration *runConfig);
    ~IRunConfigurationAspect() override;

    virtual IRunConfigurationAspect *create(RunConfiguration *runConfig) const = 0;
    virtual IRunConfigurationAspect *clone(RunConfiguration *runConfig) const;

    using RunConfigWidgetCreator = std::function<RunConfigWidget *()>;
    void setRunConfigWidgetCreator(const RunConfigWidgetCreator &runConfigWidgetCreator);
    RunConfigWidget *createConfigurationWidget() const;

    void setId(Core::Id id) { m_id = id; }
    void setDisplayName(const QString &displayName) { m_displayName = displayName; }
    void setProjectSettings(ISettingsAspect *settings);
    void setGlobalSettings(ISettingsAspect *settings);

    QString displayName() const { return m_displayName; }
    Core::Id id() const { return m_id; }
    bool isUsingGlobalSettings() const { return m_useGlobalSettings; }
    void setUsingGlobalSettings(bool value);
    void resetProjectToGlobalSettings();

    ISettingsAspect *projectSettings() const { return m_projectSettings; }
    ISettingsAspect *globalSettings() const { return m_globalSettings; }
    ISettingsAspect *currentSettings() const;
    RunConfiguration *runConfiguration() const { return m_runConfiguration; }

protected:
    friend class RunConfiguration;
    virtual void fromMap(const QVariantMap &map);
    virtual void toMap(QVariantMap &data) const;

private:
    Core::Id m_id;
    QString m_displayName;
    bool m_useGlobalSettings = false;
    RunConfiguration *m_runConfiguration = nullptr;
    ISettingsAspect *m_projectSettings = nullptr; // Owned if present.
    ISettingsAspect *m_globalSettings = nullptr;  // Not owned.
    RunConfigWidgetCreator m_runConfigWidgetCreator;
};

class PROJECTEXPLORER_EXPORT Runnable
{
    struct Concept
    {
        virtual ~Concept() {}
        virtual Concept *clone() const = 0;
        virtual bool canReUseOutputPane(const std::unique_ptr<Concept> &other) const = 0;
        virtual QString displayName() const = 0;
        virtual void *typeId() const = 0;
    };

    template <class T>
    struct Model : public Concept
    {
        Model(const T &data) : m_data(data) {}

        Concept *clone() const override { return new Model(*this); }

        bool canReUseOutputPane(const std::unique_ptr<Concept> &other) const override
        {
            if (!other.get())
                return false;
            if (other->typeId() != typeId())
                return false;
            auto that = static_cast<const Model<T> *>(other.get());
            return m_data == that->m_data;
        }

        QString displayName() const override { return m_data.displayName(); }

        void *typeId() const override { return T::staticTypeId; }

        T m_data;
    };

public:
    Runnable() = default;
    Runnable(const Runnable &other) : d(other.d ? other.d->clone() : nullptr) { }
    Runnable(Runnable &&other) : d(std::move(other.d)) {}
    template <class T> Runnable(const T &data) : d(new Model<T>(data)) {}

    void operator=(Runnable other) { d = std::move(other.d); }

    template <class T> bool is() const {
        return d.get() && (d.get()->typeId() == T::staticTypeId);
    }

    template <class T> const T &as() const {
        return static_cast<Model<T> *>(d.get())->m_data;
    }

    bool canReUseOutputPane(const Runnable &other) const;
    QString displayName() const { return d->displayName(); }

private:
    std::unique_ptr<Concept> d;
};

class PROJECTEXPLORER_EXPORT Connection
{
    struct Concept
    {
        virtual ~Concept() {}
        virtual Concept *clone() const = 0;
        virtual void *typeId() const = 0;
    };

    template <class T>
    struct Model : public Concept
    {
        Model(const T &data) : m_data(data) { }
        Concept *clone() const override { return new Model(*this); }
        void *typeId() const override { return T::staticTypeId; }
        T m_data;
    };

public:
    Connection() = default;
    Connection(const Connection &other) : d(other.d ? other.d->clone() : nullptr) { }
    Connection(Connection &&other) /* MSVC 2013 doesn't want = default */ : d(std::move(other.d)) {}
    template <class T> Connection(const T &data) : d(new Model<T>(data)) {}

    void operator=(Connection other) { d = std::move(other.d); }

    template <class T> bool is() const {
        return d.get() && (d.get()->typeId() == T::staticTypeId);
    }

    template <class T> const T &as() const {
        return static_cast<Model<T> *>(d.get())->m_data;
    }

private:
    std::unique_ptr<Concept> d;
};

// Documentation inside.
class PROJECTEXPLORER_EXPORT RunConfiguration : public ProjectConfiguration
{
    Q_OBJECT

public:
    ~RunConfiguration() override;

    virtual bool isEnabled() const;
    virtual QString disabledReason() const;
    virtual QWidget *createConfigurationWidget() = 0;

    virtual bool isConfigured() const;
    // Pop up configuration dialog in case for example the executable is missing.
    enum ConfigurationState { Configured, UnConfigured, Waiting };
    // TODO rename function
    virtual ConfigurationState ensureConfigured(QString *errorMessage = nullptr);

    Target *target() const;

    virtual Utils::OutputFormatter *createOutputFormatter() const;

    bool fromMap(const QVariantMap &map) override;
    QVariantMap toMap() const override;

    QList<IRunConfigurationAspect *> extraAspects() const;
    IRunConfigurationAspect *extraAspect(Core::Id id) const;

    template <typename T> T *extraAspect() const
    {
        foreach (IRunConfigurationAspect *aspect, m_aspects)
            if (T *result = qobject_cast<T *>(aspect))
                return result;
        return nullptr;
    }

    virtual Runnable runnable() const;
    virtual Abi abi() const;

    // Return the name of the build system target that created this run configuration.
    // May return an empty string if no target built the executable!
    virtual QString buildSystemTarget() const { return QString(); }

    void addExtraAspect(IRunConfigurationAspect *aspect);

signals:
    void enabledChanged();
    void requestRunActionsUpdate();
    void configurationFinished();

protected:
    RunConfiguration(Target *parent, Core::Id id);
    RunConfiguration(Target *parent, RunConfiguration *source);

    /// convenience function to get current build configuration.
    BuildConfiguration *activeBuildConfiguration() const;

private:
    void ctor();

    void addExtraAspects();

    QList<IRunConfigurationAspect *> m_aspects;
};

class PROJECTEXPLORER_EXPORT IRunConfigurationFactory : public QObject
{
    Q_OBJECT

public:
    explicit IRunConfigurationFactory(QObject *parent = nullptr);

    enum CreationMode {UserCreate, AutoCreate};
    virtual QList<Core::Id> availableCreationIds(Target *parent, CreationMode mode = UserCreate) const = 0;
    virtual QString displayNameForId(Core::Id id) const = 0;

    virtual bool canCreate(Target *parent, Core::Id id) const = 0;
    RunConfiguration *create(Target *parent, Core::Id id);
    virtual bool canRestore(Target *parent, const QVariantMap &map) const = 0;
    RunConfiguration *restore(Target *parent, const QVariantMap &map);
    virtual bool canClone(Target *parent, RunConfiguration *product) const = 0;
    virtual RunConfiguration *clone(Target *parent, RunConfiguration *product) = 0;

    static IRunConfigurationFactory *find(Target *parent, const QVariantMap &map);
    static IRunConfigurationFactory *find(Target *parent, RunConfiguration *rc);
    static QList<IRunConfigurationFactory *> find(Target *parent);

signals:
    void availableCreationIdsChanged();

private:
    virtual RunConfiguration *doCreate(Target *parent, Core::Id id) = 0;
    virtual RunConfiguration *doRestore(Target *parent, const QVariantMap &map) = 0;
};

class PROJECTEXPLORER_EXPORT IRunControlFactory : public QObject
{
    Q_OBJECT
public:
    explicit IRunControlFactory(QObject *parent = nullptr);

    virtual bool canRun(RunConfiguration *runConfiguration, Core::Id runMode) const;
    virtual RunControl *create(RunConfiguration *runConfiguration, Core::Id runMode, QString *errorMessage);

    virtual IRunConfigurationAspect *createRunConfigurationAspect(RunConfiguration *rc);
};

class PROJECTEXPLORER_EXPORT RunConfigWidget : public QWidget
{
    Q_OBJECT

public:
    virtual QString displayName() const = 0;

signals:
    void displayNameChanged(const QString &);
};

class PROJECTEXPLORER_EXPORT RunWorker : public QObject
{
    Q_OBJECT

public:
    explicit RunWorker(RunControl *runControl);
    ~RunWorker() override;

    RunControl *runControl() const;

    void addDependency(RunWorker *dependency);

    QString displayName() const;
    void setDisplayName(const QString &displayName);

    void setStartTimeout(int ms);
    void setStopTimeout(int ms);

    void reportData(int channel, const QVariant &data);

    void recordData(const QString &channel, const QVariant &data);
    QVariant recordedData(const QString &channel) const;

    // Part of read-only interface of RunControl for convenience.
    void appendMessage(const QString &msg, Utils::OutputFormat format);
    IDevice::ConstPtr device() const;
    const Runnable &runnable() const;
    const Connection &connection() const;
    Core::Id runMode() const;

    // States
    void initiateStart();
    void reportStarted();

    void initiateStop();
    void reportStopped();

    void reportFailure(const QString &msg = QString());

signals:
    void dataReported(int channel, const QVariant &data);
    void started();
    void stopped();

protected:
    void virtual start();
    void virtual stop();
    void virtual onFinished() {}

private:
    friend class Internal::RunControlPrivate;
    friend class Internal::RunWorkerPrivate;
    Internal::RunWorkerPrivate *d;
};

/**
 * A RunControl controls the running of an application or tool
 * on a target device. It controls start and stop, and handles
 * application output.
 *
 * RunControls are created by RunControlFactories.
 */

class PROJECTEXPLORER_EXPORT RunControl : public QObject
{
    Q_OBJECT

public:
    RunControl(RunConfiguration *runConfiguration, Core::Id mode);
    ~RunControl() override;

    void initiateStart(); // Calls start() asynchronously.
    void initiateStop(); // Calls stop() asynchronously.

    bool promptToStop(bool *optionalPrompt = nullptr) const;
    void setPromptToStop(const std::function<bool(bool *)> &promptToStop);

    virtual bool supportsReRunning() const;
    void setSupportsReRunning(bool reRunningSupported);

    virtual QString displayName() const;
    void setDisplayName(const QString &displayName);

    bool isRunning() const;
    bool isStarting() const;
    bool isStopping() const;

    void setIcon(const Utils::Icon &icon);
    Utils::Icon icon() const;

    Utils::ProcessHandle applicationProcessHandle() const;
    void setApplicationProcessHandle(const Utils::ProcessHandle &handle);
    Abi abi() const;
    IDevice::ConstPtr device() const;

    RunConfiguration *runConfiguration() const;
    Project *project() const;
    bool canReUseOutputPane(const RunControl *other) const;

    Utils::OutputFormatter *outputFormatter() const;
    Core::Id runMode() const;

    const Runnable &runnable() const;
    void setRunnable(const Runnable &runnable);

    const Connection &connection() const;
    void setConnection(const Connection &connection);

    virtual void appendMessage(const QString &msg, Utils::OutputFormat format);
    virtual void bringApplicationToForeground();

    virtual void notifyRemoteSetupDone(Utils::Port) {}  // FIXME: Replace by ToolRunner functionality
    virtual void notifyRemoteSetupFailed(const QString &) {} // Same.
    virtual void notifyRemoteFinished() {} // Same.

    static bool showPromptToStopDialog(const QString &title, const QString &text,
                                       const QString &stopButtonText = QString(),
                                       const QString &cancelButtonText = QString(),
                                       bool *prompt = nullptr);

    virtual void start();
    virtual void stop();

    using WorkerCreator = std::function<RunWorker *(RunControl *)>;
    static void registerWorkerCreator(Core::Id id, const WorkerCreator &workerCreator);
    RunWorker *workerById(Core::Id id) const;
    QList<QPointer<RunWorker>> workers() const;

    template <class T> T *worker() const {
        for (const QPointer<RunWorker> &worker : workers()) {
            if (worker) {
                if (auto res = qobject_cast<T *>(worker.data()))
                    return res;
            }
        }
        return nullptr;
    }

    RunWorker *createWorker(Core::Id id);

    using Producer = std::function<RunWorker *(RunControl *)>;
    using Constraint = std::function<bool(RunConfiguration *)>;

    template <class Worker>
    static void registerWorker(Core::Id runMode, const Constraint &constraint)
    {
        auto producer = [](RunControl *rc) { return new Worker(rc); };
        addWorkerFactory({runMode, constraint, producer});
    }
    template <class Config, class Worker>
    static void registerWorker(Core::Id runMode)
    {
        auto constraint = [](RunConfiguration *runConfig) { return qobject_cast<Config *>(runConfig); };
        auto producer = [](RunControl *rc) { return new Worker(rc); };
        addWorkerFactory({runMode, constraint, producer});
    }

    struct WorkerFactory {
        Core::Id runMode;
        Constraint constraint;
        Producer producer;
    };

    static Producer producer(RunConfiguration *runConfiguration, Core::Id runMode);

signals:
    void appendMessageRequested(ProjectExplorer::RunControl *runControl,
                                const QString &msg, Utils::OutputFormat format);
    void aboutToStart();
    void starting();
    void started();
    void finished();
    void applicationProcessHandleChanged(QPrivateSignal); // Use setApplicationProcessHandle

private:
    friend class RunWorker;
    friend class Internal::RunWorkerPrivate;

    static void addWorkerFactory(const WorkerFactory &workerFactory);

    void bringApplicationToForegroundInternal();
    Internal::RunControlPrivate *d;
};


/**
 * A simple TargetRunner for cases where a plain ApplicationLauncher is
 * sufficient for running purposes.
 */

class PROJECTEXPLORER_EXPORT SimpleTargetRunner : public RunWorker
{
public:
    explicit SimpleTargetRunner(RunControl *runControl);

    void setRunnable(const Runnable &runnable);

protected:
    void start() override;
    void stop() override;

private:
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);

    ApplicationLauncher m_launcher;
    Runnable m_runnable;
};

} // namespace ProjectExplorer
