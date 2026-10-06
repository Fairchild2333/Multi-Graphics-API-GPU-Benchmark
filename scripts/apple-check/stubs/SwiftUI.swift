// A hand-written stand-in for the parts of SwiftUI (and Combine) that the
// checked files use, so they can be type-checked on Linux. The signatures
// follow the real SDK from memory. This is NOT the real SDK: a mismatch
// between the two is not found here, only by a build with Xcode.
//
// When a checked file starts using another type or modifier, add it here with
// the signature of the real SDK. Never loosen a declaration to make an error
// go away; the errors are the point.
@_exported import Foundation

// MARK: - Combine

public protocol ObservableObject: AnyObject {}

@propertyWrapper public struct Published<Value> {
    public var wrappedValue: Value
    public init(wrappedValue: Value) { self.wrappedValue = wrappedValue }
}

// MARK: - View

@MainActor @preconcurrency public protocol View {
    associatedtype Body: View
    @ViewBuilder @MainActor @preconcurrency var body: Self.Body { get }
}

extension Never: View {
    public var body: Never { fatalError() }
}

public struct EmptyView: Sendable { public init() {} }
extension EmptyView: View { public var body: Never { fatalError() } }

public struct AnyView {}
extension AnyView: View { public var body: Never { fatalError() } }

public struct TupleView<T> {}
extension TupleView: View { public var body: Never { fatalError() } }

public struct _ConditionalContent<TrueContent, FalseContent> {}
extension _ConditionalContent: View where TrueContent: View, FalseContent: View {
    public var body: Never { fatalError() }
}

extension Optional: View where Wrapped: View {
    public var body: Never { fatalError() }
}

@resultBuilder public struct ViewBuilder {
    public static func buildExpression<Content: View>(_ content: Content) -> Content { content }
    public static func buildBlock() -> EmptyView { EmptyView() }
    public static func buildBlock<Content: View>(_ content: Content) -> Content { content }
    public static func buildBlock<each Content: View>(_ content: repeat each Content) -> TupleView<(repeat each Content)> {
        TupleView()
    }
    public static func buildIf<Content: View>(_ content: Content?) -> Content? { content }
    public static func buildEither<T: View, F: View>(first: T) -> _ConditionalContent<T, F> { _ConditionalContent() }
    public static func buildEither<T: View, F: View>(second: F) -> _ConditionalContent<T, F> { _ConditionalContent() }
    public static func buildLimitedAvailability<Content: View>(_ content: Content) -> AnyView { AnyView() }
}

// MARK: - Property wrappers

@propertyWrapper public struct Binding<Value> {
    public init(get: @escaping () -> Value, set: @escaping (Value) -> Void) {}
    public var wrappedValue: Value {
        get { fatalError() }
        nonmutating set {}
    }
    public var projectedValue: Binding<Value> { self }
}

@propertyWrapper public struct State<Value> {
    public init(wrappedValue: Value) {}
    public var wrappedValue: Value {
        get { fatalError() }
        nonmutating set {}
    }
    public var projectedValue: Binding<Value> { fatalError() }
}

public enum ColorScheme: Sendable { case light, dark }

public struct EnvironmentValues {
    public var colorScheme: ColorScheme { .light }
}

@propertyWrapper public struct Environment<Value> {
    public init(_ keyPath: KeyPath<EnvironmentValues, Value>) {}
    public var wrappedValue: Value { fatalError() }
}

@MainActor @preconcurrency @propertyWrapper public struct EnvironmentObject<ObjectType: ObservableObject> {
    public init() {}
    public var wrappedValue: ObjectType { fatalError() }
}

// MARK: - Styles and values

public protocol ShapeStyle {}

public struct Color: Hashable, Sendable, ShapeStyle {
    public enum RGBColorSpace: Sendable { case sRGB }
    public init(_ colorSpace: RGBColorSpace = .sRGB, red: Double, green: Double, blue: Double, opacity: Double = 1) {}
    public func opacity(_ opacity: Double) -> Color { self }
    public static let primary = Color(red: 0, green: 0, blue: 0)
    public static let secondary = Color(red: 0, green: 0, blue: 0)
    public static let accentColor = Color(red: 0, green: 0, blue: 0)
}
extension Color: View { public var body: Never { fatalError() } }

public struct HierarchicalShapeStyle: ShapeStyle, Sendable {}
extension ShapeStyle where Self == HierarchicalShapeStyle {
    public static var primary: HierarchicalShapeStyle { HierarchicalShapeStyle() }
    public static var secondary: HierarchicalShapeStyle { HierarchicalShapeStyle() }
}

public struct Material: ShapeStyle, Sendable {}
extension ShapeStyle where Self == Material {
    public static var ultraThinMaterial: Material { Material() }
}

public struct Font: Hashable, Sendable {
    public enum Weight: Sendable { case regular, medium, semibold, bold }
    public enum Design: Sendable { case `default`, monospaced }
    public enum TextStyle: Sendable { case largeTitle, title, title2, headline, subheadline, body, callout, caption, caption2 }
    public static let largeTitle = Font()
    public static let title2 = Font()
    public static let headline = Font()
    public static let subheadline = Font()
    public static let callout = Font()
    public static let caption = Font()
    public static let caption2 = Font()
    public static func system(size: CGFloat, weight: Weight? = nil, design: Design? = nil) -> Font { Font() }
    public static func system(_ style: TextStyle, design: Design? = nil, weight: Weight? = nil) -> Font { Font() }
}

public struct UnitPoint: Hashable, Sendable {
    public static let center = UnitPoint()
    public static let top = UnitPoint()
    public static let bottom = UnitPoint()
    public static let leading = UnitPoint()
    public static let trailing = UnitPoint()
}

public struct HorizontalAlignment: Equatable, Sendable {
    public static let leading = HorizontalAlignment()
    public static let center = HorizontalAlignment()
    public static let trailing = HorizontalAlignment()
}

public struct VerticalAlignment: Equatable, Sendable {
    public static let top = VerticalAlignment()
    public static let center = VerticalAlignment()
    public static let bottom = VerticalAlignment()
}

public struct Alignment: Equatable, Sendable {
    public static let center = Alignment()
    public static let leading = Alignment()
    public static let trailing = Alignment()
    public static let top = Alignment()
    public static let bottom = Alignment()
    public static let topLeading = Alignment()
}

public enum TextAlignment: Sendable { case leading, center, trailing }

public enum Edge: Sendable {
    case top, leading, bottom, trailing
    public struct Set: OptionSet, Sendable {
        public let rawValue: Int8
        public init(rawValue: Int8) { self.rawValue = rawValue }
        public static let top = Set(rawValue: 1)
        public static let leading = Set(rawValue: 2)
        public static let bottom = Set(rawValue: 4)
        public static let trailing = Set(rawValue: 8)
        public static let all: Set = [.top, .leading, .bottom, .trailing]
        public static let horizontal: Set = [.leading, .trailing]
        public static let vertical: Set = [.top, .bottom]
    }
}

public enum CGLineCap: Sendable { case butt, round, square }
public enum CGLineJoin: Sendable { case miter, round, bevel }

public struct StrokeStyle: Equatable {
    public init(lineWidth: CGFloat = 1, lineCap: CGLineCap = .butt, lineJoin: CGLineJoin = .miter,
                miterLimit: CGFloat = 10, dash: [CGFloat] = [CGFloat](), dashPhase: CGFloat = 0) {}
}

public struct FillStyle: Equatable, Sendable { public init() {} }

public enum RoundedCornerStyle: Sendable { case circular, continuous }

public struct Path: Equatable, Sendable {
    public init() {}
    public init(ellipseIn rect: CGRect) {}
    public init(roundedRect rect: CGRect, cornerRadius: CGFloat, style: RoundedCornerStyle = .circular) {}
    public mutating func move(to end: CGPoint) {}
    public mutating func addLine(to end: CGPoint) {}
}

public enum ColorRenderingMode: Sendable { case nonLinear, linear, extendedLinear }

public struct GraphicsContext {
    public struct Shading {
        public static func color(_ color: Color) -> Shading { Shading() }
    }
    public struct ResolvedText {
        public func measure(in size: CGSize) -> CGSize { .zero }
    }
    public func stroke(_ path: Path, with shading: Shading, style: StrokeStyle) {}
    public func stroke(_ path: Path, with shading: Shading, lineWidth: CGFloat = 1) {}
    public func fill(_ path: Path, with shading: Shading, style: FillStyle = FillStyle()) {}
    public func resolve(_ text: Text) -> ResolvedText { ResolvedText() }
    public func draw(_ text: ResolvedText, at point: CGPoint, anchor: UnitPoint = .center) {}
    public func draw(_ text: Text, at point: CGPoint, anchor: UnitPoint = .center) {}
}

// MARK: - Primitive views

public struct Text: Equatable, Sendable {
    public init<S: StringProtocol>(_ content: S) {}
    public init(verbatim content: String) {}
    public func font(_ font: Font?) -> Text { self }
    public func fontWeight(_ weight: Font.Weight?) -> Text { self }
    public func foregroundColor(_ color: Color?) -> Text { self }
}
extension Text: View { public var body: Never { fatalError() } }

public struct Image: Equatable, Sendable {
    public init(systemName: String) {}
}
extension Image: View { public var body: Never { fatalError() } }

public struct Spacer: Sendable {
    public init(minLength: CGFloat? = nil) {}
}
extension Spacer: View { public var body: Never { fatalError() } }

public protocol Shape: Sendable, View {}
extension Shape {
    public func fill<S: ShapeStyle>(_ content: S, style: FillStyle = FillStyle()) -> some View { EmptyView() }
    public func stroke<S: ShapeStyle>(_ content: S, lineWidth: CGFloat = 1) -> some View { EmptyView() }
}

public struct RoundedRectangle: Shape {
    nonisolated public init(cornerRadius: CGFloat, style: RoundedCornerStyle = .circular) {}
    public var body: Never { fatalError() }
}

public struct Circle: Shape {
    nonisolated public init() {}
    public var body: Never { fatalError() }
}

public struct VStack<Content: View> {
    @MainActor public init(alignment: HorizontalAlignment = .center, spacing: CGFloat? = nil,
                           @ViewBuilder content: () -> Content) {}
}
extension VStack: View { public var body: Never { fatalError() } }

public struct HStack<Content: View> {
    @MainActor public init(alignment: VerticalAlignment = .center, spacing: CGFloat? = nil,
                           @ViewBuilder content: () -> Content) {}
}
extension HStack: View { public var body: Never { fatalError() } }

public struct ScrollView<Content: View> {
    @MainActor public init(@ViewBuilder content: () -> Content) {}
}
extension ScrollView: View { public var body: Never { fatalError() } }

public struct ScrollViewProxy {
    public func scrollTo<ID: Hashable>(_ id: ID, anchor: UnitPoint? = nil) {}
}

public struct ScrollViewReader<Content: View> {
    @MainActor public init(@ViewBuilder content: @escaping (ScrollViewProxy) -> Content) {}
}
extension ScrollViewReader: View { public var body: Never { fatalError() } }

public struct GeometryProxy {
    public var size: CGSize { .zero }
}

public struct GeometryReader<Content: View> {
    @MainActor public init(@ViewBuilder content: @escaping (GeometryProxy) -> Content) {}
}
extension GeometryReader: View { public var body: Never { fatalError() } }

public struct GridItem: Sendable {
    public enum Size: Sendable {
        case fixed(CGFloat)
        case flexible(minimum: CGFloat = 10, maximum: CGFloat = .infinity)
        case adaptive(minimum: CGFloat, maximum: CGFloat = .infinity)
    }
    public init(_ size: Size = .flexible(), spacing: CGFloat? = nil, alignment: Alignment? = nil) {}
}

public struct PinnedScrollableViews: OptionSet, Sendable {
    public let rawValue: UInt32
    public init(rawValue: UInt32) { self.rawValue = rawValue }
}

public struct LazyVGrid<Content: View> {
    @MainActor public init(columns: [GridItem], alignment: HorizontalAlignment = .center, spacing: CGFloat? = nil,
                           pinnedViews: PinnedScrollableViews = .init(), @ViewBuilder content: () -> Content) {}
}
extension LazyVGrid: View { public var body: Never { fatalError() } }

public struct ForEach<Data: RandomAccessCollection, ID: Hashable, Content> {}
extension ForEach: View where Content: View { public var body: Never { fatalError() } }
extension ForEach where ID == Data.Element.ID, Content: View, Data.Element: Identifiable {
    @MainActor public init(_ data: Data, @ViewBuilder content: @escaping (Data.Element) -> Content) {}
}
extension ForEach where Content: View {
    @MainActor public init(_ data: Data, id: KeyPath<Data.Element, ID>,
                           @ViewBuilder content: @escaping (Data.Element) -> Content) {}
}

public struct Canvas<Symbols: View> {}
extension Canvas: View { public var body: Never { fatalError() } }
extension Canvas where Symbols == EmptyView {
    @MainActor @preconcurrency public init(opaque: Bool = false, colorMode: ColorRenderingMode = .nonLinear,
                                           rendersAsynchronously: Bool = false,
                                           renderer: @escaping (inout GraphicsContext, CGSize) -> Void) {}
}

public struct Button<Label: View> {
    @MainActor @preconcurrency public init(action: @escaping @MainActor () -> Void, @ViewBuilder label: () -> Label) {}
}
extension Button: View { public var body: Never { fatalError() } }
extension Button where Label == Text {
    @MainActor @preconcurrency public init<S: StringProtocol>(_ title: S, action: @escaping @MainActor () -> Void) {}
}

public struct Label<Title: View, Icon: View> {}
extension Label: View { public var body: Never { fatalError() } }
extension Label where Title == Text, Icon == Image {
    @MainActor public init<S: StringProtocol>(_ title: S, systemImage name: String) {}
}

public struct Picker<Label: View, SelectionValue: Hashable, Content: View> {}
extension Picker: View { public var body: Never { fatalError() } }
extension Picker where Label == Text {
    @MainActor public init<S: StringProtocol>(_ title: S, selection: Binding<SelectionValue>,
                                              @ViewBuilder content: () -> Content) {}
}

public struct TextField<Label: View> {}
extension TextField: View { public var body: Never { fatalError() } }
extension TextField where Label == Text {
    @MainActor public init<S: StringProtocol>(_ title: S, text: Binding<String>) {}
}

public struct ProgressView {
    @MainActor public init() {}
    @MainActor public init<V: BinaryFloatingPoint>(value: V?, total: V = 1.0) {}
}
extension ProgressView: View { public var body: Never { fatalError() } }

public protocol TextFieldStyle {}
public struct RoundedBorderTextFieldStyle: TextFieldStyle { public init() {} }
extension TextFieldStyle where Self == RoundedBorderTextFieldStyle {
    public static var roundedBorder: RoundedBorderTextFieldStyle { RoundedBorderTextFieldStyle() }
}

public protocol PrimitiveButtonStyle {}
public struct BorderedProminentButtonStyle: PrimitiveButtonStyle { public init() {} }
extension PrimitiveButtonStyle where Self == BorderedProminentButtonStyle {
    public static var borderedProminent: BorderedProminentButtonStyle { BorderedProminentButtonStyle() }
}

public protocol TextSelectability {}
public struct EnabledTextSelectability: TextSelectability {}
extension TextSelectability where Self == EnabledTextSelectability {
    public static var enabled: EnabledTextSelectability { EnabledTextSelectability() }
}

public enum ControlSize: Sendable { case mini, small, regular, large }
public enum UIKeyboardType: Sendable { case `default`, numberPad, decimalPad }

// MARK: - Modifiers
// View.fontWeight is left out on purpose: it needs macOS 13, and the macOS app
// supports macOS 12. Text.fontWeight above is fine.

extension View {
    public func padding(_ edges: Edge.Set = .all, _ length: CGFloat? = nil) -> some View { EmptyView() }
    public func padding(_ length: CGFloat) -> some View { EmptyView() }
    public func frame(width: CGFloat? = nil, height: CGFloat? = nil, alignment: Alignment = .center) -> some View { EmptyView() }
    public func frame(minWidth: CGFloat? = nil, idealWidth: CGFloat? = nil, maxWidth: CGFloat? = nil,
                      minHeight: CGFloat? = nil, idealHeight: CGFloat? = nil, maxHeight: CGFloat? = nil,
                      alignment: Alignment = .center) -> some View { EmptyView() }
    public func font(_ font: Font?) -> some View { EmptyView() }
    public func foregroundColor(_ color: Color?) -> some View { EmptyView() }
    public func foregroundStyle<S: ShapeStyle>(_ style: S) -> some View { EmptyView() }
    public func background<Background: View>(_ background: Background, alignment: Alignment = .center) -> some View { EmptyView() }
    public func background<S: ShapeStyle>(_ style: S) -> some View { EmptyView() }
    public func lineLimit(_ number: Int?) -> some View { EmptyView() }
    public func minimumScaleFactor(_ factor: CGFloat) -> some View { EmptyView() }
    public func fixedSize(horizontal: Bool, vertical: Bool) -> some View { EmptyView() }
    public func multilineTextAlignment(_ alignment: TextAlignment) -> some View { EmptyView() }
    public func disabled(_ disabled: Bool) -> some View { EmptyView() }
    public func labelsHidden() -> some View { EmptyView() }
    public func tag<V: Hashable>(_ tag: V) -> some View { EmptyView() }
    public func id<ID: Hashable>(_ id: ID) -> some View { EmptyView() }
    public func onAppear(perform action: (() -> Void)? = nil) -> some View { EmptyView() }
    public func onChange<V: Equatable>(of value: V, perform action: @escaping (_ newValue: V) -> Void) -> some View { EmptyView() }
    public func textFieldStyle<S: TextFieldStyle>(_ style: S) -> some View { EmptyView() }
    public func buttonStyle<S: PrimitiveButtonStyle>(_ style: S) -> some View { EmptyView() }
    public func textSelection<S: TextSelectability>(_ selectability: S) -> some View { EmptyView() }
    public func controlSize(_ controlSize: ControlSize) -> some View { EmptyView() }
    public func keyboardType(_ type: UIKeyboardType) -> some View { EmptyView() }
}

public struct Toggle<Label: View> {}
extension Toggle: View { public var body: Never { fatalError() } }
extension Toggle where Label == Text {
    @MainActor public init<S: StringProtocol>(_ title: S, isOn: Binding<Bool>) {}
}

public struct DisclosureGroup<Label: View, Content: View> {}
extension DisclosureGroup: View { public var body: Never { fatalError() } }
extension DisclosureGroup where Label == Text {
    @MainActor public init<S: StringProtocol>(_ label: S, isExpanded: Binding<Bool>, @ViewBuilder content: @escaping () -> Content) {}
}

public struct Divider: Sendable { public init() {} }
extension Divider: View { public var body: Never { fatalError() } }

public protocol ToggleStyle {}
public struct CheckboxToggleStyle: ToggleStyle { public init() {} }
public struct SwitchToggleStyle: ToggleStyle { public init() {} }
extension ToggleStyle where Self == CheckboxToggleStyle {
    public static var checkbox: CheckboxToggleStyle { CheckboxToggleStyle() }
}
extension ToggleStyle where Self == SwitchToggleStyle {
    public static var `switch`: SwitchToggleStyle { SwitchToggleStyle() }
}

public protocol PickerStyle {}
public struct MenuPickerStyle: PickerStyle { public init() {} }
public struct SegmentedPickerStyle: PickerStyle { public init() {} }
extension PickerStyle where Self == MenuPickerStyle {
    public static var menu: MenuPickerStyle { MenuPickerStyle() }
}
extension PickerStyle where Self == SegmentedPickerStyle {
    public static var segmented: SegmentedPickerStyle { SegmentedPickerStyle() }
}

extension ShapeStyle {
    public func opacity(_ opacity: Double) -> some ShapeStyle { self }
}

extension View {
    public func toggleStyle<S: ToggleStyle>(_ style: S) -> some View { EmptyView() }
    public func pickerStyle<S: PickerStyle>(_ style: S) -> some View { EmptyView() }
    public func popover<Content: View>(isPresented: Binding<Bool>, attachmentAnchor: UnitPoint = .center,
                                       arrowEdge: Edge = .top, @ViewBuilder content: @escaping () -> Content) -> some View { EmptyView() }
    public func clipShape<S: Shape>(_ shape: S, style: FillStyle = FillStyle()) -> some View { EmptyView() }
    public func overlay<Overlay: View>(_ overlay: Overlay, alignment: Alignment = .center) -> some View { EmptyView() }
    public func cornerRadius(_ radius: CGFloat, antialiased: Bool = true) -> some View { EmptyView() }
}

public struct PlainButtonStyle: PrimitiveButtonStyle { public init() {} }
extension PrimitiveButtonStyle where Self == PlainButtonStyle {
    public static var plain: PlainButtonStyle { PlainButtonStyle() }
}

extension Color {
    public static let orange = Color(red: 0, green: 0, blue: 0)
    public static let green = Color(red: 0, green: 0, blue: 0)
    public static let red = Color(red: 0, green: 0, blue: 0)
    public static let blue = Color(red: 0, green: 0, blue: 0)
    public static let gray = Color(red: 0, green: 0, blue: 0)
    public static let clear = Color(red: 0, green: 0, blue: 0)
}
