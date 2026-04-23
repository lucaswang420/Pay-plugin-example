# Payment Test Frontend Design

**Date:** 2026-04-23
**Status:** Draft
**Author:** Claude + User

## Overview

A wizard-style frontend test application for Alipay sandbox payment testing, integrated with the existing Drogon-based payment backend. The frontend guides users through the complete payment workflow: create order → select payment method → pay → refund → view refund status.

## Goals

- Provide an intuitive test interface for Alipay sandbox payment flows
- Support multiple payment methods (QR code, PC web, mobile web)
- Test complete payment lifecycle including refunds
- Simple, minimalist UI focused on functionality
- Easy to deploy and integrate with existing backend

## Non-Goals

- Production-ready payment UI (this is a test interface only)
- Multi-user support or authentication
- Advanced features like payment history, analytics dashboards
- Mobile responsive design (desktop-first is acceptable)

## Architecture

### Project Structure

```
Pay-plugin-example/
├── PayBackend/              # Existing backend
├── PayFrontend/             # New frontend project
│   ├── src/
│   │   ├── views/           # Page components for each wizard step
│   │   ├── components/      # Reusable components (StepIndicator, OrderInfo, etc.)
│   │   ├── router/          # Vue Router configuration
│   │   ├── api/             # API client (axios wrappers)
│   │   ├── stores/          # Pinia stores (order state, payment state)
│   │   ├── utils/           # Helpers (formatters, validators)
│   │   ├── App.vue          # Root component
│   │   └── main.js          # Entry point
│   ├── index.html
│   ├── package.json
│   ├── vite.config.js       # Dev server with proxy to backend
│   └── README.md
├── docker-compose.yml       # Orchestrate both services
└── README.md                # Update with frontend setup instructions
```

### Technology Stack

- **Vue 3** with Composition API (`<script setup>`)
- **Vite** for dev server and build tooling
- **Vue Router** for wizard step navigation
- **Pinia** for state management (order info, payment status)
- **Axios** for HTTP requests
- **TailwindCSS** (optional) for rapid styling

**Rationale:** This stack matches the OAuth2-plugin-example reference project, ensuring consistency across the codebase. Vite provides fast HMR during development. Vue 3 Composition API offers clean, reusable logic.

### Data Flow

```
User Input → Vue Component → Pinia Store → API Client → Backend API
                ↓                                                              ↓
         UI Update ← Response Data ← API Response ← Alipay Sandbox Client
```

**State Management:**
- `orderStore`: Stores current order info (order ID, amount, product name)
- `paymentStore`: Stores payment status, selected payment method
- Stores persist state to sessionStorage for wizard navigation

### API Integration

**Development Environment:**
- Frontend dev server: `http://localhost:5173`
- Backend server: `http://localhost:8088` (or configured port)
- Vite proxy: `/api/*` → `http://localhost:8088/api/*`

**API Endpoints:**
- `POST /api/pay/create` - Create payment order
- `GET /api/pay/query?out_trade_no={id}` - Query order status
- `POST /api/pay/refund` - Request refund
- `GET /api/pay/refund/query?out_trade_no={id}` - Query refund status

**Request/Response Format:**
- Content-Type: `application/json`
- Authentication: API key header (if required by backend filters)
- Error handling: Display error messages from backend response

## Components

### Views

#### Step1_CreateOrder.vue
- **Purpose:** Create a new payment order
- **Inputs:** Product name (text), Amount (number), Description (textarea, optional)
- **Actions:** "Create Order" button, "Reset" button
- **Outputs:** Display order ID and payment info on success
- **Validation:** Amount must be > 0, product name required

#### Step2_PaymentMethod.vue
- **Purpose:** Select payment method and initiate payment
- **Display:** Order summary (ID, amount, product name)
- **Payment Methods:**
  - QR Code Payment: Display QR code for Alipay sandbox app scan
  - PC Web Payment: "Go to Pay" button (opens Alipay in new tab)
  - Mobile Web Payment: "Go to Pay" button (mobile-optimized page)
- **Actions:** Confirm payment method selection
- **Note:** After payment initiation, auto-advance to Step 3

#### Step3_PaymentResult.vue
- **Purpose:** Display payment status and allow manual polling
- **Display:**
  - Payment status badge (Success/Pending/Failed)
  - Order details
  - "Refresh Status" button (manual polling)
  - Auto-refresh every 5 seconds (optional, can be disabled)
- **Actions:** "Next" button (enabled when payment successful)
- **Error Handling:** Show error message if payment failed, allow retry

#### Step4_Refund.vue
- **Purpose:** Request refund for successful payment
- **Display:** Order summary, refundable amount
- **Inputs:** Refund amount (default: full amount), Refund reason (optional)
- **Actions:** "Submit Refund" button
- **Validation:** Refund amount ≤ original amount, > 0

#### Step5_RefundResult.vue
- **Purpose:** Display refund status
- **Display:**
  - Refund status badge (Success/Pending/Failed)
  - Refund details (amount, reason)
  - "Refresh Status" button
- **Actions:** "Finish Test" button (resets to Step 1)

### Reusable Components

#### StepIndicator.vue
- **Purpose:** Show current step in wizard (1-2-3-4-5)
- **Visual:** Horizontal progress bar with numbered circles
- **States:** Active step highlighted, completed steps marked with checkmark

#### OrderInfoCard.vue
- **Purpose:** Display order details in a consistent format
- **Fields:** Order ID, Product Name, Amount, Created At, Status
- **Usage:** Displayed in Steps 2, 3, 4, 5

#### PaymentMethodSelector.vue
- **Purpose:** Radio button group for payment method selection
- **Options:** QR Code, PC Web, Mobile Web
- **Visual:** Icon + label for each method

#### StatusBadge.vue
- **Purpose:** Display payment/refund status with color coding
- **Variants:** Success (green), Pending (yellow), Failed (red)
- **Props:** status text, status type

## User Flow

```
1. User opens app → Step 1: Create Order
2. Fill form → Submit → Order created → Step 2: Payment Method
3. Select method → Confirm → Payment initiated → Step 3: Payment Result
4. Wait for payment → Status auto-refreshes → Payment success → Step 4: Refund
5. Fill refund form → Submit → Refund initiated → Step 5: Refund Result
6. View refund status → Click "Finish Test" → Back to Step 1
```

**Navigation Constraints:**
- Cannot skip steps (must complete in order)
- Can go back to previous steps (e.g., change payment method)
- State persists across navigation (sessionStorage)

## UI Design

### Design Principles (Minimalist)

- Clean white background (`#ffffff`)
- Black text (`#000000`) for primary content
- Gray text (`#666666`) for secondary information
- Blue accent color (`#0066cc`) for primary buttons
- Simple borders and shadows for depth
- No unnecessary graphics or animations

### Layout

- **Header:** App title ("Alipay Sandbox Payment Test")
- **Main Content:** Centered container (max-width: 800px)
- **Footer:** Copyright/usage notes
- **Step Indicator:** Top of main content

### Typography

- Headings: Sans-serif, 24-32px, bold
- Body: Sans-serif, 14-16px, regular
- Buttons: 14-16px, medium weight

### Buttons

- **Primary:** Blue background, white text, prominent
- **Secondary:** White background, black text, border
- **Disabled:** Gray background, reduced opacity
- **States:** Hover (slightly darker), Active (darker)

### Forms

- Input fields: White background, black border (1px), rounded corners
- Labels: Above input, bold
- Validation errors: Red text below field
- Focus state: Blue border

### Color Coding

- **Success:** Green (`#28a745`)
- **Pending:** Yellow/Orange (`#ffc107`)
- **Failed/Error:** Red (`#dc3545`)
- **Info:** Blue (`#0066cc`)

## Error Handling

### API Errors

- **Network Error:** "Unable to connect to server. Please check if the backend is running."
- **Validation Error:** Display field-specific errors from backend
- **Payment Failed:** Show Alipay error code and message
- **Refund Failed:** Show refund error details

### User Input Errors

- Client-side validation before API calls
- Clear error messages below input fields
- Disable submit buttons until form is valid

### Recovery Actions

- "Retry" button for failed API calls
- "Go Back" button to return to previous step
- "Reset" button to restart the wizard

## Testing

### Manual Testing Scenarios

1. **Happy Path:** Complete flow from order creation to refund
2. **Payment Failure:** Simulate failed payment (e.g., cancel payment)
3. **Partial Refund:** Refund amount less than original
4. **Navigation:** Go back and forth between steps
5. **Session Persistence:** Reload page and verify state restored

### Integration Testing

- Test against actual Alipay sandbox environment
- Verify all API endpoints return correct responses
- Test signature verification (if applicable)

## Deployment

### Development Setup

```bash
# Terminal 1: Start backend
cd PayBackend
./build/Release/payplugin

# Terminal 2: Start frontend
cd PayFrontend
npm install
npm run dev
```

Access frontend at `http://localhost:5173`

### Production Build

```bash
cd PayFrontend
npm run build
# Output: PayFrontend/dist/
```

### Deployment Strategy

**Recommended: Docker Compose (Option 2)**

This project will use Docker Compose for deployment, matching the OAuth2-plugin-example reference implementation.

**Rationale:**
- Consistent with existing plugin architecture
- Easy local development with single command
- Clear separation of frontend and backend concerns
- Simple to orchestrate both services
- Production-ready with nginx reverse proxy

**Implementation:**
```yaml
services:
  backend:
    build: ./PayBackend
    ports:
      - "8088:8088"
    environment:
      - ALIPAY_SANDBOX_APP_ID=${ALIPAY_SANDBOX_APP_ID}
      # ... other env vars

  frontend:
    build: ./PayFrontend
    ports:
      - "80:80"
    depends_on:
      - backend
    # nginx will proxy /api to backend:8088
```

**Alternative Options (Not Implemented):**
- *Option 1: Nginx Reverse Proxy* - Good for production, but requires manual nginx configuration
- *Option 3: Integrate into Drogon* - Simpler deployment but mixes concerns

## Security Considerations

- **API Key:** If backend requires authentication, include in API headers
- **CORS:** Configure backend to allow frontend origin (or use proxy in dev)
- **Environment Variables:** Store API base URL in `.env` file
- **No Sensitive Data in Frontend:** All secrets (private keys) stay in backend

## Future Enhancements (Out of Scope)

- Multi-language support (i18n)
- Payment history/test log storage
- Advanced test scenarios (partial payments, multiple refunds)
- Export test results to file
- Mobile-responsive design improvements

## Appendix: Alipay Sandbox Workflow

### Prerequisites

1. Alipay sandbox account (free registration)
2. Sandbox app credentials (APP_ID, SELLER_ID)
3. RSA key pairs configured in backend
4. Alipay sandbox app installed on mobile device (for QR code payment)

### Payment Methods

**QR Code Payment:**
1. Backend generates QR code containing payment URL
2. Frontend displays QR code
3. User opens Alipay sandbox app, scans QR code
4. Confirms payment in app
5. Frontend polls for payment success

**PC/Mobile Web Payment:**
1. Backend generates payment URL
2. Frontend opens URL in new tab/window
3. User logs in to Alipay sandbox, confirms payment
4. Payment page redirects to callback URL
5. Frontend queries payment status

### Callback Handling

Backend handles async callbacks from Alipay. Frontend polls order status endpoint to avoid missing callback updates.
