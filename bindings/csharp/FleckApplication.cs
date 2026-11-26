using System.Reflection.Metadata;
using System.Xml.Linq;

[assembly: MetadataUpdateHandler(typeof(Sunfleck.FleckCodeUpdates))]

namespace Sunfleck;

internal static class FleckCodeUpdates
{
    internal static int Version;
    public static void UpdateApplication(Type[]? _) => Interlocked.Increment(ref Version);
}

public sealed partial class App
{
    private static string ReadFleckFile(string path)
    {
        // Editors may save in place or replace the file while we inspect it.
        using var file = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
        using var reader = new StreamReader(file);
        return reader.ReadToEnd();
    }

    /// <summary>Loads a Fleck file and runs its window. Relative paths use the current directory.
    /// In watch mode, saved pages refresh transactionally and .NET Hot Reload refreshes bindings.
    /// Close the window to return. File read failures during saving are retried.</summary>
    public void RunFleckFile(string path, PropertyModel? model = null, bool watch = true)
    {
        path = Path.GetFullPath(path);
        var source = ReadFleckFile(path);
        var resourceDirectory = Path.GetDirectoryName(path)!;
        Dictionary<string, string> ReadResources() {
            var resources = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            foreach (var file in Directory.EnumerateFiles(resourceDirectory, "*.fleck", SearchOption.AllDirectories)) {
                if (Path.GetFullPath(file) == path) { continue; }
                var text = ReadFleckFile(file);
                using var xml = System.Xml.XmlReader.Create(new StringReader(text));
                try {
                    xml.MoveToContent();
                    if (xml.LocalName is "UserControl" or "ControlTemplate" or "ResourceDictionary") {
                        resources[file.Replace('\\','/')] = text;
                    }
                } catch (System.Xml.XmlException) {
                    // Pass incomplete edits to the native parser so refresh reports their location.
                    resources[file.Replace('\\','/')] = text;
                }
            }
            return resources;
        }
        var componentSources = ReadResources();
        RegisterFleckResources(componentSources);
        if (model is null) { LoadFleck(source); }
        else { LoadFleck(source, model); }
        SetFleckSourceUri(new Uri(path).AbsoluteUri);
        OpenWindow();
        if (!watch)
        {
            Run();
            return;
        }

        int codeVersion = Volatile.Read(ref FleckCodeUpdates.Version);
        long nextRead = 0;
        while (Poll(100) != Result.Exited)
        {
            int updatedVersion = Volatile.Read(ref FleckCodeUpdates.Version);
            if (updatedVersion != codeVersion)
            {
                try
                {
                    model?.NotifyPropertyChanged();
                    codeVersion = updatedVersion;
                }
                catch (SunfleckException error) when (error.Result == Result.InvalidState)
                {
                    // Retry after a transient binding callback/update has finished.
                }
            }
            if (Environment.TickCount64 < nextRead) { continue; }
            nextRead = Environment.TickCount64 + 250;
            string updated;
            try { updated = ReadFleckFile(path); }
            catch (IOException) { continue; }
            catch (UnauthorizedAccessException) { continue; }
            Dictionary<string, string> updatedComponents;
            try { updatedComponents = ReadResources(); }
            catch (IOException) { continue; }
            catch (UnauthorizedAccessException) { continue; }
            catch (System.Xml.XmlException) { continue; }
            bool componentsChanged = updatedComponents.Count != componentSources.Count || updatedComponents.Any(pair => !componentSources.TryGetValue(pair.Key, out var previous) || previous != pair.Value);
            if (updated == source && !componentsChanged) { continue; }
            var changes = updatedComponents.Select(pair => new KeyValuePair<string, string?>(pair.Key, pair.Value))
                .Concat(componentSources.Keys.Except(updatedComponents.Keys).Select(file => new KeyValuePair<string, string?>(file, null)));
            QueueFleckRefreshWithResources(updated, changes, model);
            componentSources = updatedComponents;
            source = updated;
        }
    }
}
