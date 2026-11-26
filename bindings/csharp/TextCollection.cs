using System.Collections.Specialized;
using System.Runtime.InteropServices;

namespace Sunfleck;

public sealed record DataGridColumn(string Path,string Header,double Width=120,bool Editable=true);

public enum CollectionSort { SourceOrder, Ascending, Descending }

internal static unsafe partial class Native
{
    [StructLayout(LayoutKind.Sequential)]
    internal struct GridColumn {
        [MarshalAs(UnmanagedType.LPUTF8Str)] public string Path;
        [MarshalAs(UnmanagedType.LPUTF8Str)] public string Header;
        public double Width;
        public uint Editable;
    }
    [DllImport(Library, EntryPoint="ui_data_grid_set_columns", CallingConvention=CallingConvention.Cdecl)]
    internal static extern Result GridColumns(nint grid,GridColumn[] columns,uint count);
    [DllImport(Library, EntryPoint="ui_data_grid_set_column_width", CallingConvention=CallingConvention.Cdecl)]
    internal static extern Result GridWidth(nint grid,uint column,double width);
    [DllImport(Library, EntryPoint="ui_data_grid_sort_column", CallingConvention=CallingConvention.Cdecl)]
    internal static extern Result SortColumn(nint grid,uint column,uint descending);
    [DllImport(Library, EntryPoint="ui_data_grid_finish_edit", CallingConvention=CallingConvention.Cdecl)]
    internal static extern Result FinishEdit(nint cell,uint commit);
    [DllImport(Library, EntryPoint="ui_node_set_items_source", CallingConvention=CallingConvention.Cdecl)]
    internal static extern Result SetItemsSource(nint node, nint source);
    [DllImport(Library, EntryPoint="ui_node_set_item_template", CallingConvention=CallingConvention.Cdecl)]
    internal static extern Result SetItemTemplate(nint node, [MarshalAs(UnmanagedType.LPUTF8Str)] string? label,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string? xml, nuint size);
    [DllImport(Library, EntryPoint="ui_node_get_item_identity", CallingConvention=CallingConvention.Cdecl)]
    internal static extern Result GetItemIdentity(nint node, out ulong identity);
    [DllImport(Library, EntryPoint="ui_collection_set_view", CallingConvention=CallingConvention.Cdecl)]
    internal static extern Result SetCollectionView(nint view,nint source,CollectionSort sort,[MarshalAs(UnmanagedType.LPUTF8Str)] string filter);
    [DllImport(Library, EntryPoint="ui_collection_set_group_by_label", CallingConvention=CallingConvention.Cdecl)]
    internal static extern Result SetGroupByLabel(nint view, uint enabled);
    [DllImport(Library, EntryPoint="ui_collection_group_count", CallingConvention=CallingConvention.Cdecl)]
    internal static extern Result CollectionGroupCount(nint view, out uint count);
    [DllImport(Library, EntryPoint="ui_collection_group", CallingConvention=CallingConvention.Cdecl)]
    internal static extern Result CollectionGroup(nint view, uint index, out nint key, out uint first, out uint count);
    [DllImport(Library, EntryPoint="ui_model_enable_text_collection", CallingConvention=CallingConvention.Cdecl)]
    internal static extern Result EnableTextCollection(nint model);
    [DllImport(Library, EntryPoint="ui_collection_size", CallingConvention=CallingConvention.Cdecl)]
    internal static extern Result CollectionSize(nint model, out uint count);
    [DllImport(Library, EntryPoint="ui_collection_item", CallingConvention=CallingConvention.Cdecl)]
    internal static extern Result CollectionItem(nint model, uint index, out ulong identity, out nint text);
    [DllImport(Library, EntryPoint="ui_collection_splice", CallingConvention=CallingConvention.Cdecl)]
    internal static extern Result CollectionSplice(nint model, uint index, uint removed, nint[] inserted, uint count);
    [DllImport(Library, EntryPoint="ui_collection_splice_models", CallingConvention=CallingConvention.Cdecl)]
    internal static extern Result CollectionSpliceModels(nint model, uint index, uint removed, nint[] labels, nint[] contexts, uint count);
    [DllImport(Library, EntryPoint="ui_collection_item_model", CallingConvention=CallingConvention.Cdecl)]
    internal static extern Result CollectionItemModel(nint model, uint index, out nint context);
    [DllImport(Library, EntryPoint="ui_collection_move", CallingConvention=CallingConvention.Cdecl)]
    internal static extern Result CollectionMove(nint model, uint index, uint count, uint destination);
}

public sealed partial class Node
{
    public void SetItemTemplate(string? label, string? xml) {
        if (xml is not null) { xml = Sunfleck.App.Utf8(xml); }
        App.CheckBindingResult(Native.SetItemTemplate(Handle,label is null ? null : Sunfleck.App.Utf8(label),
            xml,xml is null ? 0 : (nuint)System.Text.Encoding.UTF8.GetByteCount(xml)));
    }
    public void SetGridColumns(params DataGridColumn[] columns) {
        var native=columns.Select(c=>new Native.GridColumn { Path=Sunfleck.App.Utf8(c.Path), Header=Sunfleck.App.Utf8(c.Header), Width=c.Width, Editable=c.Editable ? 1u : 0u }).ToArray();
        SunfleckException.Check(Native.GridColumns(Handle,native,(uint)native.Length));
    }
    public void SetColumnWidth(uint column,double width) => SunfleckException.Check(Native.GridWidth(Handle,column,width));
    public void SortColumn(uint column,bool descending=false) => App.CheckBindingResult(Native.SortColumn(Handle,column,descending ? 1u : 0u));
    public void FinishCellEdit(bool commit) => App.CheckBindingResult(Native.FinishEdit(Handle,commit ? 1u : 0u));
    public void SetItemsSource(PropertyModel? source)
    {
        if (source is not null) { App.CheckOwner(source.App); }
        SunfleckException.Check(Native.SetItemsSource(Handle, source?.Handle ?? 0));
    }
}

public sealed partial class PropertyModel
{
    private IReadOnlyList<string>? textSource;
    private bool needsCollectionReset;

    public void EnableTextCollection() => SunfleckException.Check(Native.EnableTextCollection(Handle));

    public void SetCollectionView(PropertyModel source, CollectionSort sort = CollectionSort.SourceOrder, string filter = "") {
        if (textSource is not null) { throw new InvalidOperationException("The model already observes a collection."); }
        App.CheckOwner(source.App);
        App.CheckBindingResult(Native.SetCollectionView(Handle,source.Handle,sort,Sunfleck.App.Utf8(filter)));
    }

    public uint ItemCount {
        get {
            SunfleckException.Check(Native.CollectionSize(Handle, out var count));
            return count;
        }
    }
    public void SetGroupByLabel(bool enabled) => App.CheckBindingResult(Native.SetGroupByLabel(Handle,enabled ? 1u : 0u));
    public uint GroupCount {
        get {
            SunfleckException.Check(Native.CollectionGroupCount(Handle,out var count));
            return count;
        }
    }
    public (string Key, uint First, uint Count) GetGroup(uint index) {
        SunfleckException.Check(Native.CollectionGroup(Handle,index,out var key,out var first,out var count));
        return (Marshal.PtrToStringUTF8(key)!,first,count);
    }

    public (ulong Identity, string Text) GetItem(uint index) {
        SunfleckException.Check(Native.CollectionItem(Handle, index, out var identity, out var text));
        return (identity, Marshal.PtrToStringUTF8(text)!);
    }

    public void SpliceItems(uint index, uint removed, params string[] inserted) {
        if (textSource is not null) { throw new InvalidOperationException("Edit the observed collection directly."); }
        WriteItems(index, removed, inserted);
    }

    public PropertyModel? GetItemModel(uint index) {
        SunfleckException.Check(Native.CollectionItemModel(Handle,index,out var context));
        return context == 0 ? null : App.ResolveComponentModel(context);
    }

    public void SpliceModels(uint index, uint removed, params (string Label, PropertyModel? Context)[] items) {
        if (textSource is not null) { throw new InvalidOperationException("Edit the observed collection directly."); }
        var contexts = new nint[items.Length];
        var labels = new string[items.Length];
        for (int i = 0; i < items.Length; ++i) {
            var item = items[i];
            if (item.Context is not null) { App.CheckOwner(item.Context.App); }
            labels[i] = item.Label;
            contexts[i] = item.Context?.Handle ?? 0;
        }
        WriteItems(index,removed,labels,contexts);
    }

    private void WriteItems(uint index, uint removed, IReadOnlyList<string> inserted, nint[]? contexts = null) {
        var strings = new nint[inserted.Count];
        try {
            for (int i = 0; i < strings.Length; ++i) {
                strings[i] = Marshal.StringToCoTaskMemUTF8(Sunfleck.App.Utf8(inserted[i]));
            }
            App.CheckBindingResult(contexts is null ? Native.CollectionSplice(Handle, index, removed, strings, (uint)strings.Length)
                : Native.CollectionSpliceModels(Handle,index,removed,strings,contexts,(uint)strings.Length));
        } finally {
            foreach (var ptr in strings) { Marshal.FreeCoTaskMem(ptr); }
        }
    }

    public void MoveItems(uint index, uint count, uint destination) {
        if (textSource is not null) { throw new InvalidOperationException("Edit the observed collection directly."); }
        App.CheckBindingResult(Native.CollectionMove(Handle, index, count, destination));
    }

    // Source mutations and notifications belong to the UI thread.
    public void ObserveTextCollection(IReadOnlyList<string> source) {
        if (source is not INotifyCollectionChanged observable) {
            throw new ArgumentException("Source must implement INotifyCollectionChanged.", nameof(source));
        }
        EnableTextCollection();
        if (ReferenceEquals(source, textSource) && !needsCollectionReset) { return; }
        BeginUpdate();
        try {
            WriteItems(0, ItemCount, source);
            DetachTextCollection();
            textSource = source;
            needsCollectionReset = false;
            observable.CollectionChanged += TextCollectionChanged;
        } finally {
            // Publish the new contents only after their source is connected.
            EndUpdate();
        }
    }

    private void DetachTextCollection() {
        if (textSource is INotifyCollectionChanged observable) { observable.CollectionChanged -= TextCollectionChanged; }
        textSource = null;
    }

    private void TextCollectionChanged(object? sender, NotifyCollectionChangedEventArgs change) {
        if (textSource is null) { return; }
        try {
            var added = change.NewItems;
            var removed = change.OldItems;
            if (!needsCollectionReset) {
                switch (change.Action) {
                    case NotifyCollectionChangedAction.Add when change.NewStartingIndex >= 0 && added is not null:
                        WriteItems((uint)change.NewStartingIndex, 0, added.Cast<string>().ToArray());
                        return;
                    case NotifyCollectionChangedAction.Remove when change.OldStartingIndex >= 0 && removed is not null:
                        WriteItems((uint)change.OldStartingIndex, (uint)removed.Count, Array.Empty<string>());
                        return;
                    case NotifyCollectionChangedAction.Replace when change.NewStartingIndex >= 0 && change.NewStartingIndex == change.OldStartingIndex && added is not null && removed is not null:
                        WriteItems((uint)change.NewStartingIndex, (uint)removed.Count, added.Cast<string>().ToArray());
                        return;
                    case NotifyCollectionChangedAction.Move when change.OldStartingIndex >= 0 && change.NewStartingIndex >= 0 && removed is not null:
                        App.CheckBindingResult(Native.CollectionMove(Handle, (uint)change.OldStartingIndex, (uint)removed.Count, (uint)change.NewStartingIndex));
                        return;
                }
            }
            WriteItems(0, ItemCount, textSource);
            needsCollectionReset = false;
        } catch {
            needsCollectionReset = true;
            throw;
        }
    }
}
