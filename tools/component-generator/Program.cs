using System.Reflection.Metadata;
using System.Reflection.PortableExecutable;

namespace Sunfleck.Generators;

internal static class Program
{
    private static int Main(string[] args)
    {
        try {
            if (args.Length != 3 || args[0] != "export") { Console.Error.WriteLine("Usage: Sunfleck.ComponentGenerator export assembly output.fleckcomponents.json"); return 1; }
            using var input = File.OpenRead(args[1]);
            using var pe = new PEReader(input);
            var reader = pe.GetMetadataReader();
            foreach (var handle in reader.GetAssemblyDefinition().GetCustomAttributes()) {
                var attribute = reader.GetCustomAttribute(handle);
                if (attribute.Constructor.Kind != HandleKind.MemberReference) { continue; }
                var constructor = reader.GetMemberReference((MemberReferenceHandle)attribute.Constructor);
                if (constructor.Parent.Kind != HandleKind.TypeReference) { continue; }
                var type = reader.GetTypeReference((TypeReferenceHandle)constructor.Parent);
                if (reader.GetString(type.Namespace) != "Sunfleck" || reader.GetString(type.Name) != "FleckManifestAttribute") { continue; }
                var blob = reader.GetBlobReader(attribute.Value);
                if (blob.ReadUInt16() != 1) { throw new InvalidDataException("Invalid component metadata."); }
                var json = blob.ReadSerializedString() ?? throw new InvalidDataException("Missing component metadata.");
                Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(args[2]))!);
                File.WriteAllText(args[2], json);
                return 0;
            }
            throw new InvalidDataException("Assembly has no generated component manifest.");
        } catch (Exception error) { Console.Error.WriteLine(error.Message); return 1; }
    }
}
